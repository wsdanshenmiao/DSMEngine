#include <DSMEngine.h>


class Sample : public DSM::Application
{
public:

};

DSM::Application* DSM::CreateApplication()
{
    return new Sample();
}

