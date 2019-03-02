﻿// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

#include <cstdint>

#include "bb/ConvolutionIm2Col.h"
#include "bb/ConvolutionCol2Im.h"


namespace bb {


// 入力数制限Affine Binary Connect版
template <typename FT = float, typename BT = float>
class LoweringConvolution : public Layer
{
protected:
    index_t m_filter_c_size = 1;
    index_t m_filter_h_size = 1;
    index_t m_filter_w_size = 1;

    // 3層で構成
	std::shared_ptr< ConvolutionIm2Col<FT, BT> >	m_im2col;
	std::shared_ptr< Layer                     >    m_layer;
	std::shared_ptr< ConvolutionCol2Im<FT, BT> >	m_col2im;
	
	index_t 	m_im2col_size = 1;

protected:
	LoweringConvolution() {}

public:
	~LoweringConvolution() {}

    struct create_t
    {
        std::shared_ptr<Layer>  layer;
        index_t                 filter_c_size = 1;
        index_t                 filter_h_size = 1;
        index_t                 filter_w_size = 1;
    };

    static std::shared_ptr<LoweringConvolution> Create(create_t const & create)
	{
        auto self = std::shared_ptr<LoweringConvolution>(new LoweringConvolution);
        
        self->m_filter_w_size = create.filter_w_size;
        self->m_filter_h_size = create.filter_h_size;
        self->m_filter_c_size = create.filter_c_size;

  		self->m_im2col = ConvolutionIm2Col<FT, BT>::Create(filter_h_size, filter_w_size);
        self->m_layer  = create.layer;
        // col2im の形状は入力形状確定時に決まる

        return self;
	}

    static std::shared_ptr<LoweringConvolution> Create(std::shared_ptr<Layer> layer, index_t filter_c_size, index_t filter_h_size, index_t filter_w_size)
	{
        auto self = std::shared_ptr<LoweringConvolution>(new LoweringConvolution);
        
        self->m_filter_w_size = filter_w_size;
        self->m_filter_h_size = filter_h_size;
        self->m_filter_c_size = filter_c_size;

  		self->m_im2col = ConvolutionIm2Col<FT, BT>::Create(filter_h_size, filter_w_size);
        self->m_layer  = layer;
        // col2im の形状は入力形状確定時に決まる

        return self;
	}

	std::string GetClassName(void) const { return "LoweringConvolution"; }


    /**
     * @brief  コマンドを送る
     * @detail コマンドを送る
     */   
    void SendCommand(std::string command, std::string send_to = "all")
    {
	    m_im2col->SendCommand(command, send_to);
	    m_layer->SendCommand(command, send_to);
	    m_col2im->SendCommand(command, send_to);
    }
    
    /**
     * @brief  パラメータ取得
     * @detail パラメータを取得する
     *         Optimizerでの利用を想定
     * @return パラメータを返す
     */
    Variables GetParameters(void)
    {
        Variables parameters;
	    parameters.PushBack(m_im2col->GetParameters());
	    parameters.PushBack(m_layer->GetParameters());
	    parameters.PushBack(m_col2im->GetParameters());
        return parameters;
    }

    /**
     * @brief  勾配取得
     * @detail 勾配を取得する
     *         Optimizerでの利用を想定
     * @return パラメータを返す
     */
    virtual Variables GetGradients(void)
    {
        Variables gradients;
	    gradients.PushBack(m_im2col->GetGradients());
	    gradients.PushBack(m_layer->GetGradients());
	    gradients.PushBack(m_col2im->GetGradients());
        return gradients;
    }  

    /**
     * @brief  入力形状設定
     * @detail 入力形状を設定する
     *         内部変数を初期化し、以降、GetOutputShape()で値取得可能となることとする
     *         同一形状を指定しても内部変数は初期化されるものとする
     * @param  shape      1フレームのノードを構成するshape
     * @return 出力形状を返す
     */
    indices_t SetInputShape(indices_t shape)
    {
        BB_ASSERT(shape.size() == 3);

        index_t input_w_size = shape[0];
        index_t input_h_size = shape[1];
        index_t input_c_size = shape[2];
		index_t output_w_size = input_w_size - m_filter_w_size + 1;
		index_t output_h_size = input_h_size - m_filter_h_size + 1;
		index_t output_c_size = m_filter_c_size;

		m_col2im = ConvolutionCol2Im<FT, BT>::Create(output_c_size, output_h_size, output_w_size);

        shape = m_im2col->SetInputShape(shape);
        shape = m_layer->SetInputShape(shape);
        shape = m_col2im->SetInputShape(shape);

        return shape;
    }

   /**
     * @brief  forward演算
     * @detail forward演算を行う
     * @param  x     入力データ
     * @param  train 学習時にtrueを指定
     * @return forward演算結果
     */
    FrameBuffer Forward(FrameBuffer x, bool train = true)
    {
	    x = m_im2col->Forward(x, train);
	    x = m_layer->Forward(x, train);
	    x = m_col2im->Forward(x, train);
        return x;
    }

   /**
     * @brief  backward演算
     * @detail backward演算を行う
     *         
     * @return backward演算結果
     */
    FrameBuffer Backward(FrameBuffer dy)
    {
	    dy = m_col2im->Backward(dy);
	    dy = m_layer->Backward(dy);
	    dy = m_im2col->Backward(dy);
        return dy; 
    }
	

public:
	void Save(cereal::JSONOutputArchive& archive) const
	{
	    m_im2col->Save(archive);
	    m_layer->Save(archive);
	    m_col2im->Save(archive);
	}

	void Load(cereal::JSONInputArchive& archive)
	{
	    m_im2col->Load(archive);
	    m_layer->Load(archive);
	    m_col2im->Load(archive);
	}
};


}