#include <stdio.h>
#include <math.h>
#include "OutilsCreationImage.h"

// Compilation rapide depuis VS Code / terminal :
// g++ -std=c++17 main.cpp OutilsCreationImage.cpp -o saturn

// image size
#define LARGEUR 760
#define HAUTEUR 428

// saturn parameters
#define RAYON_SATURNE 150
#define CENTRE_SATURNE_X 380
#define CENTRE_SATURNE_Y 214

// Utility: pack/unpack color as 0xRRGGBBAA (consistent with OutilsCreationImage masks)
static inline unsigned long packColor(int r, int g, int b, int a = 255) {
    return ((unsigned long)(r & 0xFF) << 24) |
           ((unsigned long)(g & 0xFF) << 16) |
           ((unsigned long)(b & 0xFF) << 8) |
           ((unsigned long)(a & 0xFF));
}

static inline void unpackColor(unsigned long c, int &r, int &g, int &b, int &a) {
    r = (c >> 24) & 0xFF;
    g = (c >> 16) & 0xFF;
    b = (c >> 8) & 0xFF;
    a = c & 0xFF;
}

static inline int clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline unsigned long lerpColor(unsigned long c1, unsigned long c2, double t) {
    int r1,g1,b1,a1, r2,g2,b2,a2;
    unpackColor(c1,r1,g1,b1,a1);
    unpackColor(c2,r2,g2,b2,a2);
    int rr = clampInt((int)round(r1 + (r2 - r1) * t), 0, 255);
    int rg = clampInt((int)round(g1 + (g2 - g1) * t), 0, 255);
    int rb = clampInt((int)round(b1 + (b2 - b1) * t), 0, 255);
    int ra = clampInt((int)round(a1 + (a2 - a1) * t), 0, 255);
    return packColor(rr,rg,rb,ra);
}

// distance helper
static double dist(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx*dx + dy*dy);
}

// test ellipse membership (normalized radius)
static double ellipseNormalizedRadius(double x, double y, double xc, double yc, double a, double b) {
    double dx = x - xc;
    double dy = y - yc;
    return sqrt((dx*dx)/(a*a) + (dy*dy)/(b*b)); // 1.0 = on ellipse
}

// returns 1 if point is in ring region (between inner ellipse and outer ellipse)
static int pointInRing(double x, double y, double xc, double yc, double aInner, double bInner, double aOuter, double bOuter) {
    double r = ellipseNormalizedRadius(x,y,xc,yc,aOuter,bOuter);
    if (r > 1.0) return 0;
    double rInner = ellipseNormalizedRadius(x,y,xc,yc,aInner,bInner);
    if (rInner <= 1.0) return 0;
    return 1;
}

// returns 1 if point is in the "upper" part of the ring (y <= yc) used for painter's algorithm
static int pointInRingUpperOrLower(double x, double y, double xc, double yc, double aInner, double bInner, double aOuter, double bOuter, int upper) {
    if (!pointInRing(x,y,xc,yc,aInner,bInner,aOuter,bOuter)) return 0;
    return upper ? (y <= yc) : (y > yc);
}

// create a smooth background gradient (no starfield)
static void drawGradientBackground(unsigned long image[HAUTEUR][LARGEUR]) {
    unsigned long topColor = packColor(12, 20, 42, 255);      // deep blue at top
    unsigned long bottomColor = packColor(28, 38, 68, 255);  // slightly lighter base

    for (int y = 0; y < HAUTEUR; ++y) {
        double t = static_cast<double>(y) / (HAUTEUR - 1);
        unsigned long rowColor = lerpColor(topColor, bottomColor, t);
        for (int x = 0; x < LARGEUR; ++x) {
            image[y][x] = rowColor;
        }
    }
}

// add a soft glow around the planet to make it pop from the background
static void addPlanetGlow(unsigned long image[HAUTEUR][LARGEUR], double radius, double strength) {
    double xc = CENTRE_SATURNE_X;
    double yc = CENTRE_SATURNE_Y;
    double maxDist = radius * 2.8;

    for (int y = 0; y < HAUTEUR; ++y) {
        for (int x = 0; x < LARGEUR; ++x) {
            double d = dist(x, y, xc, yc);
            if (d > maxDist) continue;

            double t = 1.0 - (d / maxDist);
            double glow = strength * t * t;

            int r,g,b,a;
            unpackColor(image[y][x], r,g,b,a);
            r = clampInt((int)round(r + 80 * glow), 0, 255);
            g = clampInt((int)round(g + 60 * glow), 0, 255);
            b = clampInt((int)round(b + 120 * glow), 0, 255);
            image[y][x] = packColor(r,g,b,a);
        }
    }
}

// draw ring band with gradient, subtle radial streaks, and lighting
static void drawRingBand(unsigned long image[HAUTEUR][LARGEUR],
                         double aInner, double bInner, double aOuter, double bOuter,
                         unsigned long innerColor, unsigned long outerColor,
                         int upperPart) {
    double xc = CENTRE_SATURNE_X;
    double yc = CENTRE_SATURNE_Y;

    // iterate over bounding box for the outer ellipse to save time
    int minx = (int)floor(xc - aOuter) - 1;
    int maxx = (int)ceil(xc + aOuter) + 1;
    int miny = (int)floor(yc - bOuter) - 1;
    int maxy = (int)ceil(yc + bOuter) + 1;
    minx = clampInt(minx, 0, LARGEUR-1);
    maxx = clampInt(maxx, 0, LARGEUR-1);
    miny = clampInt(miny, 0, HAUTEUR-1);
    maxy = clampInt(maxy, 0, HAUTEUR-1);

    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            if (!pointInRingUpperOrLower(x,y,xc,yc,aInner,bInner,aOuter,bOuter, upperPart)) continue;

            // compute normalized radius inside ring [0..1]
            double rOuter = ellipseNormalizedRadius(x,y,xc,yc,aOuter,bOuter);
            double rInner = ellipseNormalizedRadius(x,y,xc,yc,aInner,bInner);
            double t = (rOuter - 1.0) / ((rOuter - rInner) + 1e-9);
            if (t < 0) t = 0; if (t > 1) t = 1;

            // soft radial streaks to mimic multiple ring bands
            double bandPos = (rInner - 1.0) / ((rInner - rOuter) + 1e-9);
            double streak = 0.12 * sin(30.0 * bandPos) + 0.07 * sin(70.0 * bandPos);

            // lighting: assume light from top-left; vary brightness with y and x
            double ny = (y - yc) / bOuter;
            double shade = 0.68 + 0.32 * (0.5 - ny);
            double angle = atan2((y - yc)/bOuter, (x - xc)/aOuter);
            shade += 0.14 * cos(angle - (-0.55));
            if (shade < 0.3) shade = 0.3;
            if (shade > 1.4) shade = 1.4;

            unsigned long base = lerpColor(innerColor, outerColor, t);
            int br, bg, bb, ba;
            unpackColor(base, br,bg,bb,ba);

            double bandLight = 1.0 + streak;
            br = clampInt((int)round(br * shade * bandLight), 0, 255);
            bg = clampInt((int)round(bg * shade * bandLight), 0, 255);
            bb = clampInt((int)round(bb * shade * bandLight), 0, 255);

            image[y][x] = packColor(br, bg, bb, ba);
        }
    }
}

// draw planet with radial gradient and specular highlight
static void drawPlanet(unsigned long image[HAUTEUR][LARGEUR],
                       double radius,
                       unsigned long centerColor, unsigned long edgeColor) {
    double xc = CENTRE_SATURNE_X;
    double yc = CENTRE_SATURNE_Y;

    int minx = clampInt((int)floor(xc - radius) - 1, 0, LARGEUR-1);
    int maxx = clampInt((int)ceil(xc + radius) + 1, 0, LARGEUR-1);
    int miny = clampInt((int)floor(yc - radius) - 1, 0, HAUTEUR-1);
    int maxy = clampInt((int)ceil(yc + radius) + 1, 0, HAUTEUR-1);

    // highlight center offset toward top-left
    double hx = xc - radius * 0.3;
    double hy = yc - radius * 0.35;

    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            double d = dist(x,y,xc,yc);
            if (d > radius) continue;
            double t = d / radius; // 0 center -> 1 edge
            // base gradient
            unsigned long base = lerpColor(centerColor, edgeColor, t);

            // apply rim shading (darker toward left/bottom to simulate curvature)
            double nx = (x - xc) / radius;
            double ny = (y - yc) / radius;
            double curvature = 0.7 + 0.6 * (0.5 + 0.5 * ( -nx * 0.6 - ny * 0.4 ));

            // softly layered atmospheric bands
            double lat = (y - yc) / radius; // -1 bottom, +1 top
            double banding = 1.0 + 0.10 * sin(18.0 * lat) + 0.06 * sin(40.0 * lat) + 0.04 * cos(8.0 * lat);
            double warmTint = 1.0 + 0.05 * cos(3.0 * lat);

            // specular highlight using distance from highlight point (gaussian)
            double hd = dist(x,y,hx,hy) / radius;
            double spec = exp(- (hd*hd) * 18.0);
            spec = spec * 0.9;

            int r,g,b,a;
            unpackColor(base, r,g,b,a);
            int rr = clampInt((int)round(r * curvature * banding * warmTint + 255 * spec * 0.8), 0, 255);
            int rg = clampInt((int)round(g * curvature * banding * warmTint + 190 * spec * 0.6), 0, 255);
            int rb = clampInt((int)round(b * curvature * banding + 90 * spec * 0.3), 0, 255);
            image[y][x] = packColor(rr, rg, rb, a);
        }
    }
}

int main() {
    static unsigned long image[HAUTEUR][LARGEUR];

    // Colors (RRGGBBAA)
    unsigned long planetCenter = packColor(248, 220, 150, 255); // warm bright center (pale cream)
    unsigned long planetEdge   = packColor(210, 140, 60, 255);  // deeper orange edge
    unsigned long ringInnerCol = packColor(255, 230, 180, 255); // inner bright of ring (pale beige)
    unsigned long ringOuterCol = packColor(160, 90, 40, 255);   // darker outer (brownish)

    // ring ellipses (tilted ring projected as ellipse)
    double aInner = RAYON_SATURNE * 1.2;
    double bInner = RAYON_SATURNE * 0.35;
    double aOuter = RAYON_SATURNE * 2.0;
    double bOuter = RAYON_SATURNE * 0.6;

    // 1. background without stars
    drawGradientBackground(image);
    addPlanetGlow(image, RAYON_SATURNE, 0.8);

    // 2. draw the upper half of the ring first (so it appears behind the planet)
    drawRingBand(image, aInner, bInner, aOuter, bOuter, ringInnerCol, ringOuterCol, /*upperPart=*/1);

    // 3. draw planet (over the upper ring)
    drawPlanet(image, RAYON_SATURNE, planetCenter, planetEdge);

    // 4. draw the lower half of the ring last (so it appears in front of the planet)
    drawRingBand(image, aInner, bInner, aOuter, bOuter, ringInnerCol, ringOuterCol, /*upperPart=*/0);

    // Optional: trim small areas of ring crossing the planet silhouette (softly), or draw subtle shadow of ring on planet.
    // Draw a soft shadow of the ring onto the planet (simple darker band where ring overlaps)
    double xc = CENTRE_SATURNE_X, yc = CENTRE_SATURNE_Y;
    for (int y = 0; y < HAUTEUR; ++y) {
        for (int x = 0; x < LARGEUR; ++x) {
            // if pixel belongs to planet
            if (dist(x,y,xc,yc) <= RAYON_SATURNE) {
                // check if same pixel is also inside ring (to create slight darkening)
                if (pointInRing(x,y,xc,yc,aInner,bInner,aOuter,bOuter)) {
                    int r,g,b,a;
                    unpackColor(image[y][x], r,g,b,a);
                    double shadowDepth = 0.72 + 0.18 * (0.5 - (y - yc) / (2 * bInner));
                    r = clampInt((int)(r * shadowDepth), 0, 255);
                    g = clampInt((int)(g * shadowDepth), 0, 255);
                    b = clampInt((int)(b * shadowDepth), 0, 255);
                    image[y][x] = packColor(r,g,b,a);
                }
            }
        }
    }

    // save the image using the provided OutilsCreationImage
    OutilsCreationImage::creeImage<LARGEUR>("saturn_ameliore.bmp", image, HAUTEUR);

    printf("Image 'saturn_ameliore.bmp' creee avec succes.\n");
    return 0;
}
