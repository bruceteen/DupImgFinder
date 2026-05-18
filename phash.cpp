#include <cmath>
#include <numbers>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

class DCTProcessor
{
private:
    static constexpr size_t N = 32;
    static constexpr size_t DCT_SIZE = 8;
    
    alignas(64) static float cos_table[N][DCT_SIZE];
    alignas(64) static float alpha[DCT_SIZE];
    
    struct Initializer
    {
        Initializer()
        {
            float inv_sqrt_N = 1.0f / std::sqrt(static_cast<float>(N));
            float sqrt_2_over_N = std::sqrt(2.0f / N);
            
            for( size_t u=0; u!=DCT_SIZE; ++u )
                alpha[u] = (u==0) ? inv_sqrt_N : sqrt_2_over_N;

            for( size_t x=0; x!=N; ++x )
            {
                for( size_t u=0; u<DCT_SIZE; ++u )
                {
                    cos_table[x][u] = std::cos( (2.0f*x + 1.0f) * u * std::numbers::pi_v<float> / (2.0f * N) );
                }
            }
        }
    };
    
    static Initializer init;
    
public:
    static inline uint64_t calculate( const unsigned char data[N*N] )
    {
        unsigned sum = 0;
        for( size_t i=0; i!=N*N; ++i ) sum += data[i];
        float avg = static_cast<float>(sum) / (N*N);
        
        alignas(64) float mtx[N*N];
        for( size_t i=0; i!=N*N; ++i )
            mtx[i] = static_cast<float>(data[i]) - avg;
        
        // 2. 对每一行做DCT
        alignas(64) float row_dct[N][DCT_SIZE] = {{0}};
        for( size_t y=0; y!=N; ++y ) {
            for( size_t u=0; u!=DCT_SIZE; ++u )
            {
                float sum = 0.0f;
                const float* row_ptr = &mtx[y * N];
                
                for( size_t x=0; x<N; x+=4 )
                {
                    sum += row_ptr[x+0] * cos_table[x+0][u]
                         + row_ptr[x+1] * cos_table[x+1][u]
                         + row_ptr[x+2] * cos_table[x+2][u]
                         + row_ptr[x+3] * cos_table[x+3][u];
                }
                row_dct[y][u] = sum * alpha[u];
            }
        }
        
        // 3. 对每一列做DCT
        alignas(64) float dct[DCT_SIZE][DCT_SIZE] = {{0}};
        for( size_t u=0; u!=DCT_SIZE; ++u )
        {
            for( size_t v=0; v!=DCT_SIZE; ++v )
            {
                float sum = 0.0f;
                for( size_t y=0; y<N; ++y )
                    sum += row_dct[y][u] * cos_table[y][v];
                dct[u][v] = sum * alpha[v];
            }
        }
        
        // 4. 计算低频均值并生成hash
        float dct_mean = 0.0f;
        constexpr int LOW_FREQ_SIZE = 8;
        for( int u=0; u<LOW_FREQ_SIZE; ++u )
        {
            for( int v=0; v<LOW_FREQ_SIZE; ++v )
            {
                if( u==0 && v==0 ) continue;
                dct_mean += dct[u][v];
            }
        }
        dct_mean /= 63.0f;
        
        // 5. 生成64位hash
        uint64_t hash = 0;
        for( int u=0; u!=LOW_FREQ_SIZE; ++u )
        {
            for( int v=0; v!=LOW_FREQ_SIZE; ++v )
            {
                if( u==0 && v==0 ) continue;
                hash <<= 1;
                if( dct[u][v] > dct_mean )
                    hash |= 1;
            }
        }
        
        return hash;
    }
};


alignas(64) float DCTProcessor::cos_table[DCTProcessor::N][DCTProcessor::DCT_SIZE];
alignas(64) float DCTProcessor::alpha[DCTProcessor::DCT_SIZE];
DCTProcessor::Initializer DCTProcessor::init;

uint64_t calculate_phash_32x32_fast( const unsigned char data[1024] )
{
    return DCTProcessor::calculate(data);
}
