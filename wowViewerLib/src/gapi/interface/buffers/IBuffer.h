//
// Created by Deamon on 8/27/2018.
//

#ifndef AWEBWOWVIEWERCPP_IBUFFER_H
#define AWEBWOWVIEWERCPP_IBUFFER_H

#include <memory>

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual void uploadData(const void *, int length) = 0;
    virtual void *getPointer() = 0;
    virtual void save(int length) = 0;

    virtual size_t getSize() = 0;
};

typedef std::shared_ptr<IBuffer> HGBuffer;
#endif //AWEBWOWVIEWERCPP_IBUFFER_H
