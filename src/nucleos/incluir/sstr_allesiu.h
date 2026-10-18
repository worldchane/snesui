// Reemplazo de la macro SSTR de FinalBurn Neo (libretro.cpp), que arma textos con std::ostringstream: eso trae a la
// app toda la parte de iostream de la librería de C++, con locales y el runtime de excepciones (en la consola el
// programa no abre: CE-41839-5). Esta concatena igual con "<<" sobre un std::string. La usa la copia de libretro.cpp
// que arma nucleos.sh (SSTR( → SSTR_ALLESIU().
#pragma once
#ifdef __cplusplus
#include <stdio.h>
#include <string>

struct ConcatAllesiu
{
    std::string s;
    ConcatAllesiu& operator<<(const char* t) { s += t ? t : ""; return *this; }
    ConcatAllesiu& operator<<(char* t) { s += t ? t : ""; return *this; }
    ConcatAllesiu& operator<<(const std::string& t) { s += t; return *this; }
    ConcatAllesiu& operator<<(char c) { s += c; return *this; }
    ConcatAllesiu& operator<<(double v) { char b[48]; snprintf(b, sizeof b, "%g", v); s += b; return *this; }
    ConcatAllesiu& operator<<(long long v) { char b[32]; snprintf(b, sizeof b, "%lld", v); s += b; return *this; }
    ConcatAllesiu& operator<<(unsigned long long v) { char b[32]; snprintf(b, sizeof b, "%llu", v); s += b; return *this; }
    ConcatAllesiu& operator<<(int v) { return *this << (long long)v; }
    ConcatAllesiu& operator<<(long v) { return *this << (long long)v; }
    ConcatAllesiu& operator<<(unsigned v) { return *this << (unsigned long long)v; }
    ConcatAllesiu& operator<<(unsigned long v) { return *this << (unsigned long long)v; }
};
#define SSTR_ALLESIU(x) ((ConcatAllesiu() << x).s)
#endif
