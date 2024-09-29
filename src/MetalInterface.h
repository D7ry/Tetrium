// MyObjectCInterface.h
#ifndef METAL_INTERFACE_H
#define METAL_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

void RegisterVerticalBlankingCallback(void (*callback)());

#ifdef __cplusplus
}
#endif

#endif // METAL_INTERFACE_H
