﻿// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once


#include "bb/Manager.h"
#include "bb/Activation.h"


namespace bb {


// Binarize(活性化層)
template <typename T = float>
class Binarize : public Activation<T, T>
{
protected:
    FrameBuffer m_x;
    FrameBuffer m_y;
    FrameBuffer m_dx;

    bool        m_host_only = false;

protected:
	Binarize() {}

    /**
     * @brief  コマンド処理
     * @detail コマンド処理
     * @param  args   コマンド
     */
	void CommandProc(std::vector<std::string> args)
	{
        // HostOnlyモード設定
        if (args.size() == 2 && args[0] == "host_only")
        {
            m_host_only = EvalBool(args[1]);
        }
	}


public:
    static std::shared_ptr<Binarize> Create(void)
    {
        auto self = std::shared_ptr<Binarize>(new Binarize);
        return self;
    }

	~Binarize() {}

	std::string GetClassName(void) const { return "Binarize"; }
    
    
    // ノード単位でのForward計算
    std::vector<T> ForwardNode(index_t node, std::vector<T> x_vec) const
    {
        std::vector<T> y_vec;
        for ( auto x : x_vec ) {
		    y_vec.push_back((x > (T)0.0) ? (T)1.0 : (T)0.0);
        }
        return y_vec;
    }
    

    /**
     * @brief  forward演算
     * @detail forward演算を行う
     * @param  x     入力データ
     * @param  train 学習時にtrueを指定
     * @return forward演算結果
     */
    inline FrameBuffer Forward(FrameBuffer x, bool train = true)
    {
        BB_ASSERT(x.GetType() == DataType<T>::type);

        // backwardの為に保存
        m_x = x;

        // 戻り値のサイズ設定
        m_y.ResizeLike(x);

        index_t frame_size = m_x.GetFrameSize();
        index_t node_size = m_x.GetNodeSize();

		auto x_ptr = m_x.LockConst<T>();
		auto y_ptr = m_y.Lock<T>();

		// Binarize
#pragma omp parallel for
		for (index_t node = 0; node < node_size; ++node) {
			for (index_t frame = 0; frame < frame_size; ++frame) {
				y_ptr.Set(frame, node, x_ptr.Get(frame, node) > (T)0.0 ? (T)1.0 : (T)0.0);
			}
		}

        return m_y;
    }


   /**
     * @brief  backward演算
     * @detail backward演算を行う
     *         
     * @return backward演算結果
     */
	inline FrameBuffer Backward(FrameBuffer dy)
    {
        BB_ASSERT(dy.GetType() == DataType<T>::type);

        // 戻り値のサイズ設定
        m_dx.ResizeLike(dy);

        index_t frame_size = m_dx.GetFrameSize();
        index_t node_size = m_dx.GetNodeSize();

		auto x_ptr  = m_x.LockConst<T>();
		auto y_ptr  = m_y.LockConst<T>();
		auto dy_ptr = dy.LockConst<T>();
		auto dx_ptr = m_dx.Lock<T>();
        
    	// hard-tanh
#pragma omp parallel for
		for (index_t node = 0; node < node_size; ++node) {
			for (index_t frame = 0; frame < frame_size; ++frame) {
				auto grad = dy_ptr.Get(frame, node);
				auto sig  = x_ptr.Get(frame, node);
				dx_ptr.Set(frame, node, (sig >= (T)-1.0 && sig <= (T)1.0) ? grad : 0);
			}
		}

        return m_dx;
    }
};



/**
 * @brief  forward演算
 * @detail forward演算を行う
 * @param  x     入力データ
 * @param  train 学習時にtrueを指定
 * @return forward演算結果
 */
template<>
inline FrameBuffer Binarize<float>::Forward(FrameBuffer x, bool train)
{
    BB_ASSERT(x.GetType() == BB_TYPE_FP32);

    // backwardの為に保存
    m_x = x;

    // 戻り値のサイズ設定
    m_y.ResizeLike(x);

    index_t frame_size = m_x.GetFrameSize();
    index_t node_size = m_x.GetNodeSize();

  	// Binarize
#if BB_WITH_CUDA
    if ( !m_host_only && m_x.IsDeviceAvailable() && m_y.IsDeviceAvailable() && Manager::IsDeviceAvailable() ) {
        // CUDA版
        auto ptr_x = x.LockDeviceMemoryConst();
        auto ptr_y = m_y.LockDeviceMemory();
        bbcu_fp32_Binarize_Forward(
                    (float const *)ptr_x.GetAddr(),
                    (float *)ptr_y.GetAddr(),
                    (int)node_size,
                    (int)frame_size,
                    (int)(m_x.GetFrameStride() / sizeof(float))
                );
        return m_y;
    }
#endif

    {
        // CPU版
        auto x_ptr = m_x.LockConst<float>();
	    auto y_ptr = m_y.Lock<float>();

#pragma omp parallel for
		for (index_t node = 0; node < node_size; ++node) {
			for (index_t frame = 0; frame < frame_size; ++frame) {
				y_ptr.Set(frame, node, x_ptr.Get(frame, node) > 0.0f ? 1.0f : 0.0f);
			}
		}
        return m_y;
    }
}



/**
  * @brief  backward演算
  * @detail backward演算を行う
  *         
  * @return backward演算結果
  */
template<>
inline FrameBuffer Binarize<float>::Backward(FrameBuffer dy)
{
    BB_ASSERT(dy.GetType() == BB_TYPE_FP32);
    
    // 戻り値のサイズ設定
    m_dx.ResizeLike(dy);

    index_t frame_size = m_dx.GetFrameSize();
    index_t node_size = m_dx.GetNodeSize();

    #if BB_WITH_CUDA
    if ( !m_host_only && m_x.IsDeviceAvailable() && m_dx.IsDeviceAvailable() && dy.IsDeviceAvailable() && Manager::IsDeviceAvailable() ) {
        // GPU版
        auto ptr_x  = m_x.LockDeviceMemoryConst();
        auto ptr_dy = dy.LockDeviceMemoryConst();
        auto ptr_dx = m_dx.LockDeviceMemory(true);
        bbcu_fp32_HardTanh_Backward(
                    (float const *)ptr_x.GetAddr(),
                    (float const *)ptr_dy.GetAddr(),
                    (float       *)ptr_dx.GetAddr(),
                    (int          )node_size,
                    (int          )frame_size,
                    (int          )(m_x.GetFrameStride() / sizeof(float))
                );
        return m_dx;
    }
#endif

    {
        // CPU版
        auto x_ptr  = m_x.LockConst<float>();
	    auto dy_ptr = dy.LockConst<float>();
	    auto dx_ptr = m_dx.Lock<float>(true);

#pragma omp parallel for
		for (index_t node = 0; node < node_size; ++node) {
			for (index_t frame = 0; frame < frame_size; ++frame) {
				// hard-tanh
				auto grad = dy_ptr.Get(frame, node);
				auto sig  = x_ptr.Get(frame, node);
				dx_ptr.Set(frame, node, (sig >= -1.0f && sig <= 1.0f) ? grad : 0);
			}
		}
        return m_dx;
    }
}


};

