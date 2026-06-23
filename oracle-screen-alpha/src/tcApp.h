#pragma once
#include <TrussC.h>
using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void draw() override;
private:
    int frame_ = 0;
};
