#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "OutilsCreationImage.h"

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

// create starfield on the image (random stars)
static void drawStarField(unsigned long image[HAUTEUR][LARGEUR], int seed) {
    srand(seed);
    // make background very dark near-black
    unsigned long BACK = packColor(5, 5, 12, 255); // deep space blue-black
    for (int j = 0; j < HAUTEUR; ++j)
        for (int i = 0; i < LARGEUR; ++i)
            image[j][i] = BACK;

    // sprinkle stars: different sizes and colors
    int starCount = (LARGEUR * HAUTEUR) / 400; // density, tweakable
    for (int s = 0; s < starCount; ++s) {
        int x = rand() % LARGEUR;
        int y = rand() % HAUTEUR;
        int kind = rand() % 100;
        int r,g,b;
        if (kind < 70) { r=g=b = 180 + rand()%76; } // white-ish
        else if (kind < 85) { r = 200 + rand()%56; g = 180 + rand()%76; b = 120 + rand()%80; } // warm star
        else { r = 120 + rand()%120; g = 170 + rand()%85; b = 255; } // bluish
        image[y][x] = packColor(r,g,b,255);

        // small bloom for some stars (1-2 pixels)
        if (rand()%100 < 20) {
            for (int oy=-1; oy<=1; ++oy) for (int ox=-1; ox<=1; ++ox) {
                int nx = x+ox, ny = y+oy;
                if (nx>=0 && nx < LARGEUR && ny>=0 && ny < HAUTEUR) {
                    // blend brighter for center
                    int rr,gg,bb,aa;
                    unpackColor(image[ny][nx], rr,gg,bb,aa);
                    rr = clampInt(rr + 15, 0, 255);
                    gg = clampInt(gg + 15, 0, 255);
                    bb = clampInt(bb + 15, 0, 255);
                    image[ny][nx] = packColor(rr,gg,bb,255);
                }
            }
        }
    }
}

// draw ring band with gradient and lighting
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
            // rOuter in (0,1], rInner in (0,1] but >1 for points between inner and outer
            // param t indicates closeness to outer edge (0 at inner edge -> 1 at outer)
            // we can compute interpolation factor using distances on scaled axes:
            double t = (rOuter - 1.0) / ((rOuter - rInner) + 1e-9); // safe
            if (t < 0) t = 0; if (t > 1) t = 1;

            // lighting: assume light from top-left; vary brightness with y and x
            double nx = (x - xc) / aOuter;
            double ny = (y - yc) / bOuter;
            // local normal-ish approximation for ring surface to get shading
            double shade = 0.65 + 0.35 * (0.5 - ny); // brighter on top half
            // add some elliptical curvature highlight depending on angle
            double angle = atan2((y - yc)/bOuter, (x - xc)/aOuter);
            shade += 0.12 * cos(angle - (-0.6)); // small directional highlight
            if (shade < 0.3) shade = 0.3;
            if (shade > 1.4) shade = 1.4;

            unsigned long base = lerpColor(innerColor, outerColor, t);
            int br, bg, bb, ba;
            unpackColor(base, br,bg,bb,ba);
            br = clampInt((int)round(br * shade), 0, 255);
            bg = clampInt((int)round(bg * shade), 0, 255);
            bb = clampInt((int)round(bb * shade), 0, 255);

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
            double curvature = 0.7 + 0.6 * (0.5 + 0.5 * ( -nx * 0.6 - ny * 0.4 )); // directional shading

            // specular highlight using distance from highlight point (gaussian)
            double hd = dist(x,y,hx,hy) / radius;
            double spec = exp(- (hd*hd) * 20.0); // sharper highlight
            spec = spec * 0.9; // strength

            int r,g,b,a;
            unpackColor(base, r,g,b,a);
            int rr = clampInt((int)round(r * curvature + 255 * spec * 0.8), 0, 255);
            int rg = clampInt((int)round(g * curvature + 180 * spec * 0.6), 0, 255);
            int rb = clampInt((int)round(b * curvature + 80 * spec * 0.3), 0, 255);
            image[y][x] = packColor(rr, rg, rb, a);
        }
    }
}

int main() {
    static unsigned long image[HAUTEUR][LARGEUR];

    // Colors (RRGGBBAA)
    unsigned long BG = packColor(5,5,12,255);            // very dark background (set by starfield function)
    unsigned long planetCenter = packColor(255, 220, 90, 255); // warm bright center (yellow)
    unsigned long planetEdge   = packColor(245, 130, 0, 255);  // orange edge
    unsigned long ringInnerCol = packColor(255, 220, 140, 255); // inner bright of ring (pale orange)
    unsigned long ringOuterCol = packColor(150, 40, 0, 255);   // darker outer (brownish)
    unsigned long ringBlueInner = packColor(51,153,204,255);   // alternative blue ring (if desired)

    // ring ellipses (tilted ring projected as ellipse)
    double aInner = RAYON_SATURNE * 1.2;
    double bInner = RAYON_SATURNE * 0.35;
    double aOuter = RAYON_SATURNE * 2.0;
    double bOuter = RAYON_SATURNE * 0.6;

    // 1. background + stars
    drawStarField(image, (int)time(NULL));

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
                    // darken a bit
                    r = clampInt((int)(r * 0.78), 0, 255);
                    g = clampInt((int)(g * 0.78), 0, 255);
                    b = clampInt((int)(b * 0.78), 0, 255);
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