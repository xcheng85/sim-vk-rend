#pragma once
#include <memory>
#include <mathUtils.h>
#include <scene.h>

// struct Foo // object to manage
// {
//     Foo() { std::cout << "Foo...\n"; }
//     ~Foo() { std::cout << "~Foo...\n"; }
// };

// struct D // deleter
// {
//     void operator() (Foo* p)
//     {
//         std::cout << "Calling delete for Foo object... \n";
//         delete p;
//     }
// };

using namespace vkEngine::math;

class JuliaSet : public ITexture
{
public:
    JuliaSet() = delete;
    explicit JuliaSet(
        int dim,
        float scaleFactor,
        int numIterations,
        cuComplexf c)
        : _scaleFactor{scaleFactor}, _numIterations{numIterations},
          _c{c}
    {
        _width = dim;
        _height = dim; 
        _channels = 4;
        _buffer.reset(new unsigned char[_width * _height * _channels]);
        kernel(_buffer.get());
    }
    ~JuliaSet() = default;

    virtual void *data() override
    {
        return (void*)_buffer.get();
    }

protected:
    void kernel(unsigned char *ptr)
    {
        for (int j = 0; j < _height; j++)
        {
            for (int i = 0; i < _width; i++)
            {
                // linear access, to let cpu cache friendly
                int index = i + j * _width;

                // rgba
                ptr[index * _channels] = 255 * (isInJuliaSet(i, j) ? 1 : 0);
                ptr[index * _channels + 1] = 0;
                ptr[index * _channels + 2] = 0;
                ptr[index * _channels + 3] = 255;
            }
        }
    }

    bool isInJuliaSet(int x, int y)
    {
        float jx = _scaleFactor * (float)(_width / 2 - x) / (_width / 2);
        float jy = _scaleFactor * (float)(_height / 2 - y) / (_height / 2);
        // specific to julia set equation
        cuComplexf a(jx, jy);
        for (int i = 0; i < _numIterations; i++)
        {
            a = a * a + _c;
            if (a.magnitude2() > 1000)
            {
                return false;
            }
        }
        return true;
    }

private:
    float _scaleFactor;
    int _numIterations;
    cuComplexf _c;
    std::unique_ptr<unsigned char[]> _buffer;
};