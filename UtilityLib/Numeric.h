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

inline Vector3 minimize(const Vector3 a, const Vector3 b){
	Vector3 c = a;
	for (int i = 0; i < 3; i++)	
		if (b[i] < a[i]) c[i] = b[i];

	return c;
}

inline Vector3 maximize(const Vector3 a, const Vector3 b){
	Vector3 c = a;
	for (int i = 0; i < 3; i++)	
		if (b[i] > a[i]) c[i] = b[i];

	return c;
}

inline QVector<Vector3> XYZ()
{
	QVector<Vector3> a;
	a.push_back(Vector3(1, 0, 0));
	a.push_back(Vector3(0, 1, 0));
	a.push_back(Vector3(0, 0, 1));
	return a;
}

inline double periodicalRanged(double a, double b, double v)
{
	double p = b -a;
	double n = (v - a) / p;
	if(n < 0)
		return v - p * int(n) + p;
	else
		return v - p * int(n);
}

inline double radians2degrees(double r)
{
	return 180.0 * r / M_PI;
}

inline double degrees2radians(double a)
{
	return M_PI * a / 180;
}


inline bool areCollinear(const Vector3& v0, const Vector3& v1)
{
	return cross(v0, v1).norm() < ZERO_TOLERANCE_LOW;
}


bool isPerp(const Vector3& v0, const Vector3& v1);

double dotPerp(const Vector2& v0, const Vector2& v1);

bool inRange(double t, double low, double high);

double invSqrt (double value);
void generateComplementBasis (Vector3& u, Vector3& v, const Vector3& w);
