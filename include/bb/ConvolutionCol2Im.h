﻿// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                 Copyright (C) 2018-2019 by Ryuji Fuchikami
//                                 https://github.com/ryuz
//                                 ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------


#pragma once


#include <vector>
#include <random>

#include "bb/Model.h"


namespace bb {


template <typename FT = float, typename BT = float>
class ConvolutionCol2Im : public Model
{
protected:
    indices_t       m_input_shape;

    bool            m_host_only = false;

	index_t			m_c_size = 1;
	index_t			m_h_size = 1;
	index_t			m_w_size = 1;
    
    FrameBuffer     m_y;
    FrameBuffer     m_dx;
    
protected:
	ConvolutionCol2Im() {}

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
	~ConvolutionCol2Im() {}

    struct create_t
    {
        index_t h_size = 1;
        index_t w_size = 1;
    };

	static std::shared_ptr<ConvolutionCol2Im> Create(create_t const & create)
	{
        auto self = std::shared_ptr<ConvolutionCol2Im>(new ConvolutionCol2Im);
        self->m_h_size = create.h_size;
        self->m_w_size = create.w_size;
        return self;
	}

    static std::shared_ptr<ConvolutionCol2Im> Create(index_t h_size, index_t w_size)
	{
        auto self = std::shared_ptr<ConvolutionCol2Im>(new ConvolutionCol2Im);
        self->m_h_size = h_size;
        self->m_w_size = w_size;
        return self;
	}
	
	std::string GetClassName(void) const { return "ConvolutionCol2Im"; }

	int GetChannel(void) const { return m_c_size; }
	int GetHeight(void)  const { return m_h_size; }
	int GetWidth(void)   const { return m_w_size; }
	

public:

    /**
     * @brief  入力のshape設定
     * @detail 入力のshape設定
     * @param shape 新しいshape
     * @return なし
     */
    indices_t SetInputShape(indices_t shape)
    {
        BB_ASSERT(shape.size() == 1);
        m_input_shape  = shape;
        m_c_size = shape[0];
        return indices_t({m_w_size, m_h_size, m_c_size});
    }

    /**
     * @brief  入力形状取得
     * @detail 入力形状を取得する
     * @return 入力形状を返す
     */
    indices_t GetInputShape(void) const
    {
        return m_input_shape;
    }

    /**
     * @brief  出力形状取得
     * @detail 出力形状を取得する
     * @return 出力形状を返す
     */
    indices_t GetOutputShape(void) const
    {
        return indices_t({m_w_size, m_h_size, m_c_size});
    }


    FrameBuffer Forward(FrameBuffer x, bool train=true)
 	{
        BB_ASSERT(x.GetType() == DataType<FT>::type);

       	index_t input_frame_size  = x.GetFrameSize();
        BB_ASSERT(input_frame_size % (m_h_size * m_w_size) == 0);
    	index_t output_frame_size = input_frame_size / (m_h_size * m_w_size);

        m_y.Resize(DataType<FT>::type, output_frame_size, indices_t({m_w_size, m_h_size, m_c_size}));


#ifdef BB_WITH_CUDA
        if ( !m_host_only && DataType<FT>::type == BB_TYPE_FP32 && x.IsDeviceAvailable() && m_y.IsDeviceAvailable() && Manager::IsDeviceAvailable() ) {
            // FP32 CUDA
            auto x_ptr = x.LockDeviceMemoryConst();
            auto y_ptr = m_y.LockDeviceMemory(true);

            bbcu_fp32_Col2Im_Forward
                (
                    (float const *)x_ptr.GetAddr(),
                    (float       *)y_ptr.GetAddr(),
                    (int          )m_w_size,
                    (int          )m_h_size,
                    (int          )m_c_size,
                    (int          )(x.GetFrameStride() / sizeof(float)),
                    (int          )m_y.GetFrameSize(),
                    (int          )(m_y.GetFrameStride() / sizeof(float))
                );

            return m_y;
        }
#endif

#ifdef BB_WITH_CUDA
        if ( !m_host_only && DataType<FT>::type == BB_TYPE_BIT && x.IsDeviceAvailable() && m_y.IsDeviceAvailable() && Manager::IsDeviceAvailable() ) {
            // Bit CUDA
            auto x_ptr = x.LockDeviceMemoryConst();
            auto y_ptr = m_y.LockDeviceMemory(true);

            bbcu_bit_Col2Im_Forward
                (
                    (int const *)x_ptr.GetAddr(),
                    (int       *)y_ptr.GetAddr(),
                    (int        )m_w_size,
                    (int        )m_h_size,
                    (int        )m_c_size,
                    (int        )(x.GetFrameStride() / sizeof(int)),
                    (int        )m_y.GetFrameSize(),
                    (int        )(m_y.GetFrameStride() / sizeof(int))
                );

            return m_y;
        }
#endif

        {
            // 汎用版
            auto x_ptr = x.LockConst<FT>();
            auto y_ptr = m_y.Lock<FT>(true);
		    index_t input_frame = 0;
		    for (index_t output_frame = 0; output_frame < output_frame_size; ++output_frame) {
			    for (index_t y = 0; y < m_h_size; ++y) {
				    for (index_t x = 0; x < m_w_size; ++x) {
					    #pragma omp parallel for
					    for (index_t c = 0; c < m_c_size; ++c) {
						    index_t input_node = c;
						    index_t output_node = (c*m_h_size + y)*m_w_size + x;
						    y_ptr.Set(output_frame, output_node, x_ptr.Get(input_frame, input_node));
					    }
					    ++input_frame;
				    }
			    }
		    }
            return m_y;
        }
	}
	
	FrameBuffer Backward(FrameBuffer dy)
	{
        BB_ASSERT(dy.GetType() == DataType<BT>::type);

    	index_t output_frame_size = dy.GetFrameSize();
       	index_t input_frame_size  = output_frame_size *(m_h_size * m_w_size);

        m_dx.Resize(DataType<BT>::type, input_frame_size, m_c_size);

#ifdef BB_WITH_CUDA
        if ( !m_host_only && DataType<FT>::type == BB_TYPE_FP32 && dy.IsDeviceAvailable() && m_dx.IsDeviceAvailable() && Manager::IsDeviceAvailable() )
        {
            auto dy_ptr = dy.LockDeviceMemoryConst();
            auto dx_ptr = m_dx.LockDeviceMemory(true);

            bbcu_fp32_Col2Im_Backward
                (
                    (float const *)dy_ptr.GetAddr(),
                    (float       *)dx_ptr.GetAddr(),
                    (int          )m_w_size,
                    (int          )m_h_size,
                    (int          )m_c_size,
                    (int          )(m_dx.GetFrameStride() / sizeof(float)),
                    (int          )dy.GetFrameSize(),
                    (int          )(dy.GetFrameStride() / sizeof(float))
                );

            return m_dx;
        }
#endif

        {
		    auto dy_ptr = dy.LockConst<BT>();
		    auto dx_ptr = m_dx.Lock<BT>(true);

		    index_t input_frame = 0;
		    for (index_t output_frame = 0; output_frame < output_frame_size; ++output_frame) {
			    for (index_t y = 0; y < m_h_size; ++y) {
				    for (index_t x = 0; x < m_w_size; ++x) {
					    #pragma omp parallel for
					    for (index_t c = 0; c < m_c_size; ++c) {
						    index_t output_node = (c*m_h_size + y)*m_w_size + x;
						    index_t input_node = c;
						    dx_ptr.Set(input_frame, input_node, dy_ptr.Get(output_frame, output_node));
					    }
					    ++input_frame;
				    }
			    }
		    }
            return m_dx;
        }
	}
};


}