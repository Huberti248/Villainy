#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include <SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
//#include <SDL_gpu.h>
//#include <SFML/Network.hpp>
//#include <SFML/Graphics.hpp>
#include <algorithm>
#include <atomic>
#include <codecvt>
#include <functional>
#include <locale>
#include <mutex>
#include <thread>
#ifdef __ANDROID__
#include "vendor/PUGIXML/src/pugixml.hpp"
#include <android/log.h> //__android_log_print(ANDROID_LOG_VERBOSE, "Villainy", "Example number log: %d", number);
#include <jni.h>
#else
#include <filesystem>
#include <pugixml.hpp>
#ifdef __EMSCRIPTEN__
namespace fs = std::__fs::filesystem;
#else
namespace fs = std::filesystem;
#endif
using namespace std::chrono_literals;
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// NOTE: Remember to uncomment it on every release
//#define RELEASE

#if defined _MSC_VER && defined RELEASE
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

//240 x 240 (smart watch)
//240 x 320 (QVGA)
//360 x 640 (Galaxy S5)
//640 x 480 (480i - Smallest PC monitor)

int windowWidth = 240;
int windowHeight = 320;
SDL_Point mousePos;
SDL_Point realMousePos;
bool keys[SDL_NUM_SCANCODES];
bool buttons[SDL_BUTTON_X2 + 1];
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* robotoF;
std::string prefPath;
bool running = true;

void logOutputCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    std::cout << message << std::endl;
}

int random(int min, int max)
{
    return min + rand() % ((max + 1) - min);
}

int SDL_QueryTextureF(SDL_Texture* texture, Uint32* format, int* access, float* w, float* h)
{
    int wi, hi;
    int result = SDL_QueryTexture(texture, format, access, &wi, &hi);
    if (w) {
        *w = wi;
    }
    if (h) {
        *h = hi;
    }
    return result;
}

SDL_bool SDL_PointInFRect(const SDL_Point* p, const SDL_FRect* r)
{
    return ((p->x >= r->x) && (p->x < (r->x + r->w)) && (p->y >= r->y) && (p->y < (r->y + r->h))) ? SDL_TRUE : SDL_FALSE;
}

std::ostream& operator<<(std::ostream& os, SDL_FRect r)
{
    os << r.x << " " << r.y << " " << r.w << " " << r.h;
    return os;
}

std::ostream& operator<<(std::ostream& os, SDL_Rect r)
{
    SDL_FRect fR;
    fR.w = r.w;
    fR.h = r.h;
    fR.x = r.x;
    fR.y = r.y;
    os << fR;
    return os;
}

SDL_Texture* renderText(SDL_Texture* previousTexture, TTF_Font* font, SDL_Renderer* renderer, const std::string& text, SDL_Color color)
{
    if (previousTexture) {
        SDL_DestroyTexture(previousTexture);
    }
    SDL_Surface* surface;
    if (text.empty()) {
        surface = TTF_RenderUTF8_Blended(font, " ", color);
    }
    else {
        surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    }
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return texture;
    }
    else {
        return 0;
    }
}

struct Text {
    std::string text;
    SDL_Surface* surface = 0;
    SDL_Texture* t = 0;
    SDL_FRect dstR{};
    bool autoAdjustW = false;
    bool autoAdjustH = false;
    float wMultiplier = 1;
    float hMultiplier = 1;

    ~Text()
    {
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
    }

    Text()
    {
    }

    Text(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
    }

    Text& operator=(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
        return *this;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Color c = { 255, 255, 255 })
    {
        this->text = text;
#if 1 // NOTE: renderText
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (text.empty()) {
            surface = TTF_RenderUTF8_Blended(font, " ", c);
        }
        else {
            surface = TTF_RenderUTF8_Blended(font, text.c_str(), c);
        }
        if (surface) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
#endif
        if (autoAdjustW) {
            SDL_QueryTextureF(t, 0, 0, &dstR.w, 0);
        }
        if (autoAdjustH) {
            SDL_QueryTextureF(t, 0, 0, 0, &dstR.h);
        }
        dstR.w *= wMultiplier;
        dstR.h *= hMultiplier;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, int value, SDL_Color c = { 255, 255, 255 })
    {
        setText(renderer, font, std::to_string(value), c);
    }

    void draw(SDL_Renderer* renderer)
    {
        if (t) {
            SDL_RenderCopyF(renderer, t, 0, &dstR);
        }
    }
};

int SDL_RenderDrawCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y - offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

int SDL_RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {

        status += SDL_RenderDrawLine(renderer, x - offsety, y + offsetx,
            x + offsety, y + offsetx);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y + offsety,
            x + offsetx, y + offsety);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y - offsety,
            x + offsetx, y - offsety);
        status += SDL_RenderDrawLine(renderer, x - offsety, y - offsetx,
            x + offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

struct Clock {
    Uint64 start = SDL_GetPerformanceCounter();

    float getElapsedTime()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        return secondsElapsed * 1000;
    }

    float restart()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        start = SDL_GetPerformanceCounter();
        return secondsElapsed * 1000;
    }
};

SDL_bool SDL_FRectEmpty(const SDL_FRect* r)
{
    return ((!r) || (r->w <= 0) || (r->h <= 0)) ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IntersectFRect(const SDL_FRect* A, const SDL_FRect* B, SDL_FRect* result)
{
    int Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    if (!result) {
        SDL_InvalidParamError("result");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        result->w = 0;
        result->h = 0;
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    result->x = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->w = Amax - Amin;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    result->y = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->h = Amax - Amin;

    return (SDL_FRectEmpty(result) == SDL_TRUE) ? SDL_FALSE : SDL_TRUE;
}

SDL_bool SDL_HasIntersectionF(const SDL_FRect* A, const SDL_FRect* B)
{
    int Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    return SDL_TRUE;
}

enum class State {
    Gameplay,
    Shop,
};

struct Shop {
    SDL_FRect speedR;
    SDL_FRect moreCoinsR;
    SDL_FRect exitR;
    SDL_FRect heartR;
    SDL_FRect damageR;
    Text speedPriceText;
    Text heartPriceText;
    Text moreCoinsPriceText;
    Text damagePriceText;
};

struct Enemy {
    SDL_FRect r{};
    int hp = 0;
    Clock attackClock;
    bool attacking = false;
};

int maxHearths = 3;
SDL_Texture* wagonT;
SDL_Texture* playerT;
SDL_Texture* treasureChestT;
SDL_Texture* coinT;
SDL_Texture* shopT;
SDL_Texture* speedT;
SDL_Texture* exitT;
SDL_Texture* gameplayBackgroundT;
SDL_Texture* shopBackgroundT;
SDL_Texture* soundMutedT;
SDL_Texture* soundUnmutedT;
SDL_Texture* enemyT;
SDL_Texture* heartT;
SDL_Texture* creditsT;
std::vector<SDL_Texture*> attackTextures;
SDL_FRect wagonR;
SDL_FRect playerR;
SDL_FRect treasureChestR;
SDL_FRect soundMutedAndUnmutedR;
SDL_FRect creditsR;
float dx = 0;
float dy = 0;
int playerSpeed = 1;
int playerDamage = 1;
int coinGain = 1;
Clock globalClock;
Text coinsText;
bool hasCoin = false;
std::vector<SDL_FRect> coinRects;
SDL_FRect shopR;
State state = State::Gameplay;
Shop s;
bool isPlayerRight = true;
int attackFrame = -1;
bool soundMuted = false;
Mix_Music* music;
Mix_Chunk* hitS;
Mix_Chunk* pickupS;
Mix_Chunk* powerupS;
std::vector<Enemy> enemies;
Clock enemyClock;
Text playerHpText;
bool showCredits = false;

void save()
{
    pugi::xml_document doc;
    pugi::xml_node rootNode = doc.append_child("root");
    pugi::xml_node coinsNode = rootNode.append_child("coins");
    coinsNode.append_child(pugi::node_pcdata).set_value(coinsText.text.c_str());
    pugi::xml_node speedNode = rootNode.append_child("speed");
    speedNode.append_child(pugi::node_pcdata).set_value(std::to_string(playerSpeed).c_str());
    pugi::xml_node coinGainNode = rootNode.append_child("coinGain");
    coinGainNode.append_child(pugi::node_pcdata).set_value(std::to_string(coinGain).c_str());
    pugi::xml_node maxHearthsNode = rootNode.append_child("maxHearths");
    maxHearthsNode.append_child(pugi::node_pcdata).set_value(std::to_string(maxHearths).c_str());
    pugi::xml_node playerDamageNode = rootNode.append_child("playerDamage");
    playerDamageNode.append_child(pugi::node_pcdata).set_value(std::to_string(playerDamage).c_str());
    pugi::xml_node soundMutedNode = rootNode.append_child("soundMuted");
    soundMutedNode.append_child(pugi::node_pcdata).set_value(std::to_string(soundMuted ? 1 : 0).c_str());
    doc.save_file((prefPath + "data.xml").c_str());
}

int eventWatch(void* userdata, SDL_Event* event)
{
    // WARNING: Be very careful of what you do in the function, as it may run in a different thread
    if (event->type == SDL_APP_TERMINATING || event->type == SDL_APP_WILLENTERBACKGROUND) {
        save();
    }
    return 0;
}

void movePlayer(float deltaTime)
{
    dx = 0;
    dy = 0;
    if (keys[SDL_SCANCODE_A]) {
        dx = -1;
        isPlayerRight = false;
    }
    else if (keys[SDL_SCANCODE_D]) {
        dx = 1;
        isPlayerRight = true;
    }
    else if (keys[SDL_SCANCODE_W]) {
        dy = -1;
    }
    else if (keys[SDL_SCANCODE_S]) {
        dy = 1;
    }
    playerR.x += dx * playerSpeed * deltaTime;
    playerR.y += dy * playerSpeed * deltaTime;
    if (playerR.x < 0) {
        playerR.x = 0;
    }
    if (playerR.y < 0) {
        playerR.y = 0;
    }
    if (playerR.x + playerR.w > windowWidth) {
        playerR.x = windowWidth - playerR.w;
    }
    if (playerR.y + playerR.h > windowHeight) {
        playerR.y = windowHeight - playerR.h;
    }
}

void muteMusicAndSounds()
{
    Mix_VolumeMusic(0);
    Mix_Volume(-1, 0);
}

void unmuteMusicAndSounds()
{
    Mix_VolumeMusic(16);
    Mix_Volume(-1, 32);
}

void mainLoop()
{
    float deltaTime = globalClock.restart();
    if (state == State::Gameplay) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
                if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    attackFrame = 0;
                    Mix_PlayChannel(-1, hitS, 0);
                    for (int i = 0; i < enemies.size(); ++i) {
                        if (SDL_HasIntersectionF(&playerR, &enemies[i].r)) {
                            enemies[i].hp -= playerDamage;
                            if (enemies[i].hp <= 0) {
                                enemies.erase(enemies.begin() + i--);
                            }
                        }
                    }
                }
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        movePlayer(deltaTime);
        if (SDL_HasIntersectionF(&playerR, &treasureChestR)) {
            hasCoin = true;
        }
        if (SDL_HasIntersectionF(&playerR, &wagonR)) {
            if (hasCoin) {
                hasCoin = false;
                for (int i = 0; i < coinGain; ++i) {
                    coinRects.push_back(SDL_FRect());
                    coinRects.back().w = 32;
                    coinRects.back().h = 32;
                    coinRects.back().x = random(wagonR.x, wagonR.x + wagonR.w - coinRects.back().w);
                    coinRects.back().y = random(wagonR.y, wagonR.y + wagonR.h - coinRects.back().h);
                    coinsText.setText(renderer, robotoF, std::to_string(std::stoi(coinsText.text) + 1));
                }
                Mix_PlayChannel(-1, pickupS, 0);
            }
        }
        if (SDL_HasIntersectionF(&playerR, &shopR)) {
            state = State::Shop;
            playerR.x = windowWidth / 2 - playerR.w / 2;
            playerR.y = windowHeight / 2 - playerR.h / 2;
            coinsText.dstR.y = windowHeight - coinsText.dstR.h - 5;
        }
        if (SDL_HasIntersectionF(&playerR, &soundMutedAndUnmutedR)) {
            soundMuted = !soundMuted;
            if (soundMuted) {
                muteMusicAndSounds();
            }
            else {
                unmuteMusicAndSounds();
            }
        }
        if (SDL_HasIntersectionF(&playerR, &creditsR)) {
            showCredits = true;
        }
        else {
            showCredits = false;
        }
        if (enemyClock.getElapsedTime() > 10000) {
            enemies.push_back(Enemy());
            enemies.back().r.w = 32;
            enemies.back().r.h = 32;
            int randomNumber = random(0, 3);
            if (randomNumber == 0) {
                enemies.back().r.x = -enemies.back().r.w;
                enemies.back().r.y = random(-enemies.back().r.h, windowHeight);
            }
            else if (randomNumber == 1) {
                enemies.back().r.x = random(-enemies.back().r.w, windowWidth);
                enemies.back().r.y = -enemies.back().r.h;
            }
            else if (randomNumber == 2) {
                enemies.back().r.x = windowWidth;
                enemies.back().r.y = random(-enemies.back().r.h, windowHeight);
            }
            else if (randomNumber == 3) {
                enemies.back().r.x = random(-enemies.back().r.h, windowWidth);
                enemies.back().r.y = windowHeight;
            }
            enemies.back().hp = random(2, 3);
            enemyClock.restart();
        }
        for (int i = 0; i < enemies.size(); ++i) {
            float dx = 0;
            float dy = 0;
            if (enemies[i].r.x < playerR.x) {
                dx = 1;
            }
            else if (enemies[i].r.x + enemies[i].r.w > playerR.x + playerR.w) {
                dx = -1;
            }
            if (enemies[i].r.y < playerR.y) {
                dy = 1;
            }
            else if (enemies[i].r.y + enemies[i].r.h > playerR.y + playerR.h) {
                dy = -1;
            }
            enemies[i].r.x += dx * 0.1 * deltaTime;
            enemies[i].r.y += dy * 0.1 * deltaTime;
        }
        for (int i = 0; i < enemies.size(); ++i) {
            if (SDL_HasIntersectionF(&playerR, &enemies[i].r)) {
                if (!enemies[i].attacking) {
                    enemies[i].attacking = true;
                    enemies[i].attackClock.restart();
                }
                else if (enemies[i].attackClock.getElapsedTime() > 3000) {
                    playerHpText.setText(renderer, robotoF, std::stoi(playerHpText.text) - 1, { 255, 0, 0 });
                    if (std::stoi(playerHpText.text) <= 0) {
                        coinsText.setText(renderer, robotoF, "0");
                        playerHpText.setText(renderer, robotoF, maxHearths, { 255, 0, 0 });
                    }
                    enemies[i].attackClock.restart();
                }
            }
            else {
                enemies[i].attacking = false;
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, gameplayBackgroundT, 0, 0);
        SDL_RenderCopyF(renderer, wagonT, 0, &wagonR);
        SDL_RenderCopyF(renderer, treasureChestT, 0, &treasureChestR);
        for (int i = 0; i < coinRects.size(); ++i) {
            SDL_RenderCopyF(renderer, coinT, 0, &coinRects[i]);
        }
        SDL_RenderCopyF(renderer, shopT, 0, &shopR);
        SDL_RenderCopyF(renderer, creditsT, 0, &creditsR);
        coinsText.draw(renderer);
        SDL_RenderCopyF(renderer, soundMuted ? soundMutedT : soundUnmutedT, 0, &soundMutedAndUnmutedR);
        for (int i = 0; i < enemies.size(); ++i) {
            SDL_RenderCopyF(renderer, enemyT, 0, &enemies[i].r);
        }
        playerHpText.draw(renderer);
        SDL_RenderCopyExF(renderer, playerT, 0, &playerR, 0, 0, isPlayerRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
        if (attackFrame >= 0) {
            SDL_RenderCopyF(renderer, attackTextures[attackFrame], 0, &playerR);
            if (++attackFrame >= attackTextures.size()) {
                attackFrame = -1;
            }
        }
        if (showCredits) {
            std::vector<Text> creditTexts;
            creditTexts.push_back(coinsText);
            creditTexts.back().wMultiplier = 0.13;
            creditTexts.back().dstR.x = 0;
            creditTexts.back().dstR.y = coinsText.dstR.y + coinsText.dstR.h;
            creditTexts.back().setText(renderer, robotoF, "https://opengameart.org/content/thief-or-robber");

            creditTexts.push_back(creditTexts.back());
            creditTexts.back().dstR.y = creditTexts[creditTexts.size() - 2].dstR.y + creditTexts[creditTexts.size() - 2].dstR.h;
            creditTexts.back().setText(renderer, robotoF, "https://opengameart.org/content/medieval-cart-prop");

            creditTexts.push_back(creditTexts.back());
            creditTexts.back().dstR.y = creditTexts[creditTexts.size() - 2].dstR.y + creditTexts[creditTexts.size() - 2].dstR.h;
            creditTexts.back().setText(renderer, robotoF, "Freepik");

            creditTexts.push_back(creditTexts.back());
            creditTexts.back().dstR.y = creditTexts[creditTexts.size() - 2].dstR.y + creditTexts[creditTexts.size() - 2].dstR.h;
            creditTexts.back().setText(renderer, robotoF, "Smashicons");

            creditTexts.push_back(creditTexts.back());
            creditTexts.back().dstR.y = creditTexts[creditTexts.size() - 2].dstR.y + creditTexts[creditTexts.size() - 2].dstR.h;
            creditTexts.back().setText(renderer, robotoF, "Pixel perfect");

            for (int i = 0; i < creditTexts.size(); ++i) {
                creditTexts[i].draw(renderer);
            }
        }
        SDL_RenderPresent(renderer);
    }
    else if (state == State::Shop) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        movePlayer(deltaTime);
        if (SDL_HasIntersectionF(&playerR, &s.exitR)) {
            state = State::Gameplay;
            playerR.x = windowWidth / 2 - playerR.w / 2;
            playerR.y = windowHeight / 2 - playerR.h / 2;
            coinsText.dstR.y = 5;
        }
        if (SDL_HasIntersectionF(&playerR, &s.speedR)) {
            if (std::stoi(coinsText.text) >= 100) {
                coinsText.setText(renderer, robotoF, std::stoi(coinsText.text) - 100);
                for (int i = 0; i < 100; ++i) {
                    coinRects.erase(coinRects.begin());
                }
                ++playerSpeed;
                Mix_PlayChannel(-1, powerupS, 0);
            }
        }
        if (SDL_HasIntersectionF(&playerR, &s.heartR)) {
            if (std::stoi(coinsText.text) >= 150) {
                coinsText.setText(renderer, robotoF, std::stoi(coinsText.text) - 150);
                for (int i = 0; i < 150; ++i) {
                    coinRects.erase(coinRects.begin());
                }
                ++maxHearths;
                playerHpText.setText(renderer, robotoF, maxHearths, { 255, 0, 0 });
                Mix_PlayChannel(-1, powerupS, 0);
            }
        }
        if (SDL_HasIntersectionF(&playerR, &s.moreCoinsR)) {
            if (std::stoi(coinsText.text) >= 200) {
                coinsText.setText(renderer, robotoF, std::stoi(coinsText.text) - 200);
                for (int i = 0; i < 200; ++i) {
                    coinRects.erase(coinRects.begin());
                }
                ++coinGain;
                Mix_PlayChannel(-1, powerupS, 0);
            }
        }
        if (SDL_HasIntersectionF(&playerR, &s.damageR)) {
            if (std::stoi(coinsText.text) >= 300) {
                coinsText.setText(renderer, robotoF, std::stoi(coinsText.text) - 300);
                for (int i = 0; i < 300; ++i) {
                    coinRects.erase(coinRects.begin());
                }
                ++playerDamage;
                Mix_PlayChannel(-1, powerupS, 0);
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, shopBackgroundT, 0, 0);
        SDL_RenderCopyF(renderer, speedT, 0, &s.speedR);
        s.speedPriceText.draw(renderer);
        s.heartPriceText.draw(renderer);
        SDL_RenderCopyF(renderer, exitT, 0, &s.exitR);
        SDL_RenderCopyF(renderer, coinT, 0, &s.moreCoinsR);
        SDL_RenderCopyF(renderer, heartT, 0, &s.heartR);
        SDL_RenderCopyF(renderer, attackTextures.front(), 0, &s.damageR);
        s.moreCoinsPriceText.draw(renderer);
        coinsText.draw(renderer);
        s.damagePriceText.draw(renderer);
        SDL_RenderCopyExF(renderer, playerT, 0, &playerR, 0, 0, isPlayerRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char* argv[])
{
    std::srand(std::time(0));
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    SDL_LogSetOutputFunction(logOutputCallback, 0);
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    SDL_GetMouseState(&mousePos.x, &mousePos.y);
    prefPath = SDL_GetPrefPath("Huberti", "Villainy");
    window = SDL_CreateWindow("Villainy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    robotoF = TTF_OpenFont("res/roboto.ttf", 72);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_RenderSetScale(renderer, w / (float)windowWidth, h / (float)windowHeight);
    SDL_AddEventWatch(eventWatch, 0);
    wagonT = IMG_LoadTexture(renderer, "res/cart_top-down/td-cart_00000.png");
    playerT = IMG_LoadTexture(renderer, "res/thief/thief_steal2.png");
    treasureChestT = IMG_LoadTexture(renderer, "res/treasureChest.png");
    coinT = IMG_LoadTexture(renderer, "res/coins/coin_01.png");
    shopT = IMG_LoadTexture(renderer, "res/shop.png");
    speedT = IMG_LoadTexture(renderer, "res/speed.png");
    exitT = IMG_LoadTexture(renderer, "res/exit.png");
    gameplayBackgroundT = IMG_LoadTexture(renderer, "res/gameplayBackground.png");
    shopBackgroundT = IMG_LoadTexture(renderer, "res/shopBackground.png");
    soundMutedT = IMG_LoadTexture(renderer, "res/soundMuted.png");
    soundUnmutedT = IMG_LoadTexture(renderer, "res/soundUnmuted.png");
    enemyT = IMG_LoadTexture(renderer, "res/Free monster sprite sheets/Transparent PNG/Idle/frame-1.png");
    heartT = IMG_LoadTexture(renderer, "res/heart.png");
    creditsT = IMG_LoadTexture(renderer, "res/credits.png");
    attackTextures.push_back(IMG_LoadTexture(renderer, "res/Everything/Alternative 2/1/Alternative_2_01.png"));
    attackTextures.push_back(IMG_LoadTexture(renderer, "res/Everything/Alternative 2/1/Alternative_2_02.png"));
    attackTextures.push_back(IMG_LoadTexture(renderer, "res/Everything/Alternative 2/1/Alternative_2_03.png"));
    attackTextures.push_back(IMG_LoadTexture(renderer, "res/Everything/Alternative 2/1/Alternative_2_04.png"));
    wagonR.w = 100;
    wagonR.h = 100;
    wagonR.x = 0;
    wagonR.y = windowHeight - wagonR.h;
    playerR.w = 50;
    playerR.h = 50;
    playerR.x = windowWidth / 2 - playerR.w / 2;
    playerR.y = windowHeight / 2 - playerR.h / 2;
    treasureChestR.w = 50;
    treasureChestR.h = 50;
    treasureChestR.x = 0;
    treasureChestR.y = 0;
    coinsText.autoAdjustW = true;
    coinsText.wMultiplier = 0.5;
    coinsText.setText(renderer, robotoF, "0");
    coinsText.dstR.h = 20;
    coinsText.dstR.x = windowWidth / 2 - coinsText.dstR.w / 2;
    coinsText.dstR.y = 5;
    shopR.w = 64;
    shopR.h = 64;
    shopR.x = windowWidth - shopR.w;
    shopR.y = windowHeight - shopR.h;
    soundMutedAndUnmutedR.w = 32;
    soundMutedAndUnmutedR.h = 32;
    soundMutedAndUnmutedR.x = windowWidth - soundMutedAndUnmutedR.w;
    soundMutedAndUnmutedR.y = 0;
    playerHpText.autoAdjustW = true;
    playerHpText.wMultiplier = 0.5;
    playerHpText.setText(renderer, robotoF, "3", { 255, 0, 0 });
    playerHpText.dstR.h = 20;
    playerHpText.dstR.x = windowWidth / 2 - playerHpText.dstR.w / 2;
    playerHpText.dstR.y = coinsText.dstR.y + coinsText.dstR.h;
    creditsR.w = 32;
    creditsR.h = 32;
    creditsR.x = windowWidth - creditsR.w - 5;
    creditsR.y = windowHeight / 2 - creditsR.h / 2;
    s.damageR.w = 64;
    s.damageR.h = 64;
    s.damageR.x = 5;
    s.damageR.y = windowHeight - s.damageR.h - 20;
    s.speedR.w = 32;
    s.speedR.h = 32;
    s.speedR.x = 5;
    s.speedR.y = 5;
    s.speedPriceText.autoAdjustW = true;
    s.speedPriceText.wMultiplier = 0.2;
    s.speedPriceText.setText(renderer, robotoF, "100", {});
    s.speedPriceText.dstR.h = 20;
    s.speedPriceText.dstR.x = s.speedR.x + s.speedR.w / 2 - s.speedPriceText.dstR.w / 2;
    s.speedPriceText.dstR.y = s.speedR.y + s.speedR.h;
    s.exitR.w = 64;
    s.exitR.h = 64;
    s.exitR.x = windowWidth - s.exitR.w;
    s.exitR.y = windowHeight - s.exitR.h - 5;
    s.moreCoinsR.w = 32;
    s.moreCoinsR.h = 32;
    s.moreCoinsR.x = windowWidth - s.moreCoinsR.w - 5;
    s.moreCoinsR.y = 5;
    s.heartPriceText.autoAdjustW = true;
    s.heartPriceText.wMultiplier = 0.2;
    s.heartPriceText.setText(renderer, robotoF, "150", {});
    s.heartPriceText.dstR.h = 20;
    s.heartPriceText.dstR.x = windowWidth / 2 - s.heartPriceText.dstR.w / 2;
    s.heartPriceText.dstR.y = s.moreCoinsR.y + s.moreCoinsR.h;
    s.moreCoinsPriceText.autoAdjustW = true;
    s.moreCoinsPriceText.wMultiplier = 0.2;
    s.moreCoinsPriceText.setText(renderer, robotoF, "200", {});
    s.moreCoinsPriceText.dstR.h = 20;
    s.moreCoinsPriceText.dstR.x = s.moreCoinsR.x + s.moreCoinsR.w / 2 - s.moreCoinsPriceText.dstR.w / 2;
    s.moreCoinsPriceText.dstR.y = s.moreCoinsR.y + s.moreCoinsR.h;
    s.damagePriceText.autoAdjustW = true;
    s.damagePriceText.wMultiplier = 0.2;
    s.damagePriceText.setText(renderer, robotoF, "300", {});
    s.damagePriceText.dstR.h = 20;
    s.damagePriceText.dstR.x = s.damageR.x + s.damageR.w / 2 - s.damagePriceText.dstR.w / 2;
    s.damagePriceText.dstR.y = s.damageR.y + s.damageR.h;
    s.heartR.w = 32;
    s.heartR.h = 32;
    s.heartR.x = windowWidth / 2 - s.heartR.w / 2;
    s.heartR.y = 5;
    music = Mix_LoadMUS("res/music.wav");
    hitS = Mix_LoadWAV("res/hit.wav");
    pickupS = Mix_LoadWAV("res/pickup.wav");
    powerupS = Mix_LoadWAV("res/powerup.wav");

    {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file((prefPath + "data.xml").c_str());
        if (result) {
            coinsText.setText(renderer, robotoF, doc.child("root").child("coins").text().as_string());
            for (int i = 0; i < std::stoi(coinsText.text); ++i) {
                coinRects.push_back(SDL_FRect());
                coinRects.back().w = 32;
                coinRects.back().h = 32;
                coinRects.back().x = random(wagonR.x, wagonR.x + wagonR.w - coinRects.back().w);
                coinRects.back().y = random(wagonR.y, wagonR.y + wagonR.h - coinRects.back().h);
            }
            playerSpeed = doc.child("root").child("speed").text().as_int();
            coinGain = doc.child("root").child("coinGain").text().as_int();
            maxHearths = doc.child("root").child("maxHearths").text().as_int();
            playerHpText.setText(renderer, robotoF, maxHearths, { 255, 0, 0 });
            playerDamage = doc.child("root").child("playerDamage").text().as_int();
            soundMuted = doc.child("root").child("soundMuted").text().as_int();
        }
    }

    if (soundMuted) {
        muteMusicAndSounds();
    }
    else {
        unmuteMusicAndSounds();
    }
    Mix_PlayMusic(music, -1);

    globalClock.restart();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (running) {
        mainLoop();
    }
#endif
    // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
    save();
    return 0;
}