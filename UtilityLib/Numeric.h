#pragma once

#include "UtilityGlobal.h"

#include <cstdlib>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <limits>
#include <QtAlgorithms>

// utility macros
// alert: keep in mind a macro do NOT check type 
// and make copies of the parameters, which is extremely dangerous if the parameter is generated by calling a function. 
#define Max(a,b) (((a) > (b)) ? (a) : (b))
#define Min(a,b) (((a) < (b)) ? (a) : (b))
#define RANGED(min, v, max) ( Max(min, Min(v, max)) ) 

// Tolerance
#define ZERO_TOLERANCE_LOW 1e-06
#define ZERO_TOLERANCE_HIGH 1e-01

Vector3 minimize(const Vector3 a, const Vector3 b);
Vector3 maximize(const Vector3 a, const Vector3 b);

QVector<Vector3> XYZ();
double periodicalRanged(double a, double b, double v);

double radians2degrees(double r);
double degrees2radians(double a);

bool areCollinear(const Vector3& v0, const Vector3& v1);
bool isPerp(const Vector3& v0, const Vector3& v1);

double dotPerp(const Vector2& v0, const Vector2& v1);

bool inRange(double t, double low, double high);

double invSqrt (double value);
void generateComplementBasis (Vector3& u, Vector3& v, const Vector3& w);

Vector3 perpVector(const Vector3& n); 

double minDouble();
double maxDouble();
bool solveQuadratic( double a, double b, double c, double& root1, double &root2 );