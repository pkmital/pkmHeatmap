/*
 
 pkmHeatmap.h
 
 The MIT License (MIT)
 
 Copyright (c) 2015 Parag K. Mital, http://pkmital.com
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 
 */

#pragma once

#include "ofMain.h"

// this isn't working all the time, sometimes crashes on accessing textureTarget... 
// no idea why but it doesn't seem much faster anyway... leave it commented.
#define USE_REDUCTION_CHAIN

class pkmColormap
{
public:
    
    enum colormode {
        heatmap_jet,
        heatmap_hot,
        heatmap_cool,
        heatmap_gray
    };
    
    pkmColormap()
    {
        max_value = 1.0;
        setColorMap(heatmap_jet);
    }
    
    void setColorMap(colormode c)
    {
        if (c == heatmap_jet) {
            colormap.load(ofToDataPath("jetmap"));
        }
        else if (c == heatmap_hot) {
            colormap.load(ofToDataPath("hotmap"));
        }
        else if (c == heatmap_cool) {
            colormap.load(ofToDataPath("coolmap"));
        }
        else if (c == heatmap_gray) {
            colormap.load(ofToDataPath("graymap"));
        }
    }
    
    void setMaxValue(float m)
    {
        max_value = m;
    }
    
    void begin(ofTexture &ref)
    {
        colormap.begin();
        colormap.setUniformTexture("image", ref, 0);
        colormap.setUniform1f("maxValue", max_value);
    }
    
    void end()
    {
        colormap.end();
    }
    
    ofShader colormap;
    float max_value;
    
};

class pkmHeatmap 
{

public:
    
    enum colormode {
        heatmap_jet,
        heatmap_hot,
        heatmap_cool,
        heatmap_gray
    };
    

    pkmHeatmap()
    {
        bNormalize = true;
        kernelStepSize = 1;
        scalar = 3;                 // 3
        circleRadius = 8;           // 8
        numPasses = 3;              // 3

        
        gaussShader.load(ofToDataPath("cp.gaussian.2p"));
        gaussShader.begin();
        gaussShader.setUniform2f("width", 0.0f, 0.0f);
        //gaussShader.setUniform1f("sigma", 15.0f);
        //gaussShader.setUniform1f("blurSize", 1.0f);
        gaussShader.end();
        
#ifdef USE_REDUCTION_CHAIN
        reduceShader.load(ofToDataPath("maxreduction"));
#endif
        
        setColorMap(heatmap_jet);
    }
    
    void setColorMap(colormode c)
    {
        if (c == heatmap_jet) {
            colormap.load(ofToDataPath("jetmap"));
        }
        else if (c == heatmap_hot) {
            colormap.load(ofToDataPath("hotmap"));
        }
        else if (c == heatmap_cool) {
            colormap.load(ofToDataPath("coolmap"));
        }
        else if (c == heatmap_gray) {
            colormap.load(ofToDataPath("graymap"));
        }
    }
    
    void allocate(int w, int h)
    {
        width = w/scalar;
        height = h/scalar;
        
        ofFbo::Settings settings;
        settings.minFilter = GL_LINEAR;
        settings.maxFilter = GL_LINEAR;
        settings.width = width;
        settings.height = height;
        settings.internalformat = GL_RGBA32F_ARB;
        settings.numSamples = 0;
        settings.useDepth = false;
        settings.useStencil = false;
        
        heatmap.allocate(settings);
        heatmap2.allocate(settings);
        impulses.allocate(settings);
        
        settings.width = w;
        settings.height = h;
        settings.internalformat = GL_RGBA32F_ARB;
        finalFbo.allocate(settings);
        
        heatmap.begin();
        glClear( GL_COLOR_BUFFER_BIT );
        heatmap.end();
        
        heatmap2.begin();
        glClear( GL_COLOR_BUFFER_BIT );
        heatmap2.end();
        
        impulses.begin();
        glClear( GL_COLOR_BUFFER_BIT );
        impulses.end();
        
        
#ifdef USE_REDUCTION_CHAIN
        w = width;
        h = height;
        int i = 0;
        while (w != 1 || h != 1) {
            w = max(w/2.0,1.0);
            h = max(h/2.0,1.0);
            settings.width = w;
            settings.height = h;
            reductionChain.push_back(ofFbo());
            reductionChain.back().allocate(settings);
            
            reductionChain.back().begin();
            ofSetColor(0);
            ofDrawRectangle(0, 0, w, h);
            reductionChain.back().end();
            i++;
        }
#endif
        
    }
    
    void setScalar(float scale)
    {
        scalar = scale;
    }
    
    void setGaussianWidth(float width)
    {
        kernelStepSize = width;
    }
    
    void toggleNormalization()
    {
        bNormalize = !bNormalize;
    }
    
    void addPoints(vector<int> x, vector<int> y)
    {
        impulses.begin();
        ofBackground(0);
        ofSetColor(255);
        ofSetRectMode(OF_RECTMODE_CENTER);
        for (int i = 0; i < x.size(); i++) {
            ofDrawCircle(x[i] / scalar, y[i] / scalar, circleRadius);
        }
        ofSetRectMode(OF_RECTMODE_CORNER);
        impulses.end();
        
    }
    
    void resetHeatmap()
    {
        impulses.begin();
        glClear( GL_COLOR_BUFFER_BIT );
        impulses.end();
    }
    
    void update()
    {
        heatmap2.begin();
        impulses.draw(0,0);
        heatmap2.end();
        
        for (int i = 0; i < numPasses; i++) {
            
            heatmap.begin();
            gaussShader.begin();
            gaussShader.setUniformTexture("image", heatmap2.getTexture(), 1);
            gaussShader.setUniform2f("width", 0.0f, kernelStepSize);
            heatmap2.draw(0, 0);
            gaussShader.end();
            heatmap.end();
            
            heatmap2.begin();
            gaussShader.begin();
            gaussShader.setUniformTexture("image", heatmap.getTexture(), 0);
            gaussShader.setUniform2f("width", kernelStepSize, 0.0f);
            heatmap.draw(0, 0);
            gaussShader.end();
            heatmap2.end();
            
        }
        
        float maxValue = 1.0;
        if (bNormalize) {
            maxValue = getMaxValue();
        }
        
        finalFbo.begin();
        ofBackground(0);
        colormap.begin();
        colormap.setUniformTexture("image", heatmap2.getTexture(), 0);
        colormap.setUniform1f("maxValue", maxValue);
        heatmap2.draw(0, 0, width*scalar, height*scalar);
        colormap.end();
        finalFbo.end();
    }
    
    void draw()
    {   
        finalFbo.draw(0,0);
    }
    
    ofTexture & getTextureReference()
    {
        return finalFbo.getTexture();
    }
    
protected:
    
    
    float getMaxValue()
    {
        ofFloatPixels pixels;
#ifdef USE_REDUCTION_CHAIN
        reductionChain[0].begin();
        reduceShader.begin();
        reduceShader.setUniformTexture("image", heatmap2.getTexture(), 0);
        heatmap2.draw(0, 0);
        reduceShader.end();
        reductionChain[0].end();
        
        int w = width;
        int h = height;
        int i = 1;
        while (i < reductionChain.size()) {
            reductionChain[i].begin();
            reduceShader.begin();
            reduceShader.setUniformTexture("image", reductionChain[i-1].getTexture(), 0);
            reductionChain[i-1].draw(0, 0, reductionChain[i].getWidth(), reductionChain[i].getHeight());
            reduceShader.end();
            reductionChain[i].end();
            i++;
        }
        
        reductionChain.back().readToPixels(pixels);
#else   
        heatmap2.readToPixels(pixels);
#endif
        float *pix = pixels.getData();
        float maxValue = pix[0];
        for (int i = 0; i < pixels.size(); i++) {
            if (pix[i] > maxValue) {
                maxValue = pix[i];
            }
        }
        return maxValue;
    }
    
private:
    float           circleRadius;
    float           kernelStepSize, sigma, numPixelsPerDegree;
    float           scalar;
    int             width, height;
    int             numPasses;
    ofFbo           heatmap, heatmap2, finalFbo, impulses;
    vector<ofFbo>   reductionChain;
    ofShader        gaussShader;
    ofShader        colormap;
    ofShader        reduceShader;
    bool            bNormalize;
};
