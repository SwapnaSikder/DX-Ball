// dxball_fixed.c
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib") // For PlaySound

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ================= CONSTANTS =================
#define WINDOW_W 800
#define WINDOW_H 600
#define PADDLE_H 15
#define BALL_R 8
#define MAX_BRICKS 100
#define MAX_PARTICLES 200
#define MAX_POWERUPS 10
#define MAX_BULLETS 10

// ================= ENUMS =================
enum GameState { MENU, PLAYING, PAUSED, GAMEOVER, HIGHSCORE, WIN, HELP };
enum PowerType { EXTRA_LIFE, FASTER_BALL, WIDER_PADDLE, FIREBALL, THROUGH_BRICK, SHRINK_PADDLE, IMMEDIATE_DEATH };

// ================= STRUCTS =================
typedef struct { float x,y,w,h; int alive,colorIdx,hp; } Brick;
typedef struct { float x,y,vx,vy,r; int active; int fireball; int throughBrick; } Ball;
typedef struct { float x,y,r,vx,vy; int alive; float life; } Particle;
typedef struct { float x,y,vy; int type,alive; } PowerUp;
typedef struct { float x,y,vy; int alive; } Bullet;

// ================= GLOBALS =================
Brick bricks[MAX_BRICKS]; int brickCount;
Ball ball; int ballLaunched = 0;
float paddleX, paddleW = 80, paddleY = 50;
int leftDown=0,rightDown=0;

int score=0,lives=3,highscore=0,level=1;
enum GameState state=MENU;

Particle particles[MAX_PARTICLES]; int particleCount=0;
PowerUp powers[MAX_POWERUPS]; int powerCount=0;

Bullet bullets[MAX_BULLETS]; int bulletCount = 0;

int menuSelection=0;
#define MENU_ITEMS 5

float dx=4.0f, dy=4.0f;

int bgPlaying = 0; // is background music playing?

// ================= HELPERS =================
float clampf(float v,float a,float b){ if(v<a) return a; if(v>b) return b; return v; }
void drawString(float x,float y,const char *s){
    glRasterPos2f(x,y);
    for(const char* p=s;*p;p++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*p);
}
void drawRect(float x,float y,float w,float h){
    glBegin(GL_QUADS);
    glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}
void saveHighscore(){ FILE*f=fopen("highscore.dat","w"); if(f){fprintf(f,"%d",highscore); fclose(f);} }
void loadHighscore(){ FILE*f=fopen("highscore.dat","r"); if(f){fscanf(f,"%d",&highscore); fclose(f);} }

void stopBackground(){
    if(bgPlaying){
        PlaySound(NULL, NULL, SND_PURGE);
        bgPlaying = 0;
    }
}
void startBackground(){
    if(!bgPlaying){
        PlaySound(TEXT("background.wav"), NULL, SND_ASYNC | SND_LOOP);
        bgPlaying = 1;
    }
}

// ================= GAME INIT =================
void clearArrays(){
    for(int i=0;i<MAX_PARTICLES;i++) particles[i].alive = 0;
    for(int i=0;i<MAX_POWERUPS;i++) powers[i].alive = 0;
    for(int i=0;i<MAX_BULLETS;i++) bullets[i].alive = 0;
    particleCount = 0; powerCount = 0; bulletCount = 0;
}

void initBricks(){
    brickCount=0;
    int rows=5 + level; // increase rows as level increases
    int cols=10;
    float bw=60,bh=20,startX=80,startY=WINDOW_H-120;
    // clamp to MAX_BRICKS
    for(int r=0;r<rows && brickCount < MAX_BRICKS; r++){
        for(int c=0;c<cols && brickCount < MAX_BRICKS; c++){
            Brick b;
            b.x = startX + c*(bw+5);
            b.y = startY - r*(bh+5);
            b.w = bw; b.h = bh;
            b.alive = 1; b.colorIdx = rand()%5; b.hp = 1 + rand()%2;
            bricks[brickCount++] = b;
        }
    }
}

void resetBall(){
    ball.x = paddleX + paddleW/2.0f;
    ball.y = paddleY + PADDLE_H + BALL_R;
    ball.r = BALL_R;
    ball.vx = 0; ball.vy = 0;
    ball.active = 1; ball.fireball = 0; ball.throughBrick = 0;
    ballLaunched = 0;
}

void startNewGame(){
    level = 1;
    score = 0; lives = 3;
    paddleW = 80;
    clearArrays();
    initBricks();
    resetBall();
    state = PLAYING;
    startBackground();
}

void resumeGame(){
    state = PLAYING;
    startBackground();
}

// ================= PARTICLES =================
void spawnParticles(float x,float y){
    for(int i=0;i<10 && particleCount<MAX_PARTICLES;i++){
        Particle p; p.x=x; p.y=y; p.r=3;
        float ang=((float)(rand()%360))*M_PI/180.0f;
        float spd=((float)(rand()%30))/10.0f + 1.0f;
        p.vx = cosf(ang)*spd; p.vy = sinf(ang)*spd; p.life = 1.0f; p.alive = 1;
        particles[particleCount++] = p;
    }
}
void updateParticles(){
    for(int i=0;i<particleCount;i++){
        if(!particles[i].alive) continue;
        particles[i].x += particles[i].vx; particles[i].y += particles[i].vy;
        particles[i].life -= 0.03f;
        if(particles[i].life<=0) particles[i].alive=0;
    }
}

// ================= POWERUPS =================
void spawnPower(float x,float y){
    if(powerCount>=MAX_POWERUPS) return;
    if(rand()%3==0){
        PowerUp p; p.x=x; p.y=y; p.vy = -1.6f;
        p.type = rand()%7; p.alive=1;
        powers[powerCount++] = p;
    }
}
void updatePowers(){
    for(int i=0;i<powerCount;i++){
        if(!powers[i].alive) continue;
        powers[i].y += powers[i].vy;
        if(powers[i].y < 0) powers[i].alive=0;
        if(powers[i].x > paddleX && powers[i].x < paddleX + paddleW &&
           powers[i].y < paddleY + PADDLE_H && powers[i].y > paddleY){
            switch(powers[i].type){
                case EXTRA_LIFE: { lives++; PlaySound(TEXT("powerup.wav"), NULL, SND_ASYNC); break; }
                case FASTER_BALL: { ball.vx*=1.2f; ball.vy*=1.2f; PlaySound(TEXT("powerup.wav"), NULL, SND_ASYNC); break; }
                case WIDER_PADDLE: { paddleW+=30; PlaySound(TEXT("powerup.wav"), NULL, SND_ASYNC); break; }
                case SHRINK_PADDLE: { paddleW = fmax(40,paddleW-30); PlaySound(TEXT("powerup.wav"), NULL, SND_ASYNC); break; }
                case FIREBALL: { ball.fireball=1; PlaySound(TEXT("powerup.wav"), NULL, SND_ASYNC); break; }
                case THROUGH_BRICK: { ball.throughBrick=1; PlaySound(TEXT("powerup.wav"), NULL, SND_ASYNC); break; }
                case IMMEDIATE_DEATH: { lives--; PlaySound(TEXT("hit.wav"), NULL, SND_ASYNC); break; }
            }
            powers[i].alive = 0;
        }
    }
}

// ================= BULLETS =================
void spawnBullet(float x,float y){
    for(int i=0;i<MAX_BULLETS;i++){
        if(!bullets[i].alive){
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].vy = 6.0f;
            bullets[i].alive = 1;
            bulletCount++;
            PlaySound(TEXT("shoot.wav"), NULL, SND_ASYNC);
            break;
        }
    }
}
void updateBullets(){
    for(int i=0;i<MAX_BULLETS;i++){
        if(!bullets[i].alive) continue;
        bullets[i].y += bullets[i].vy;
        if(bullets[i].y > WINDOW_H){ bullets[i].alive=0; bulletCount--; continue; }
        // Check collision with bricks
        for(int j=0;j<brickCount;j++){
            if(!bricks[j].alive) continue;
            if(bullets[i].x > bricks[j].x && bullets[i].x < bricks[j].x + bricks[j].w &&
               bullets[i].y > bricks[j].y && bullets[i].y < bricks[j].y + bricks[j].h){
                bricks[j].hp--;
                if(bricks[j].hp<=0){
                    bricks[j].alive = 0;
                    score += 10;
                    spawnParticles(bullets[i].x, bullets[i].y);
                    spawnPower(bullets[i].x, bullets[i].y);
                }
                bullets[i].alive = 0; bulletCount--;
                PlaySound(TEXT("hit.wav"), NULL, SND_ASYNC);
                break;
            }
        }
    }
}
void renderBullets(){
    glColor3f(1.0,1.0,1.0);
    for(int i=0;i<MAX_BULLETS;i++){
        if(!bullets[i].alive) continue;
        drawRect(bullets[i].x-2, bullets[i].y-8, 4, 8);
    }
}

// ================= COLLISIONS =================
void checkCollisions(){
    if(ball.y - ball.r < paddleY + PADDLE_H &&
       ball.x > paddleX && ball.x < paddleX + paddleW && ball.vy < 0){
        ball.vy = fabsf(ball.vy);
        float hitPos = (ball.x - (paddleX + paddleW/2.0f)) / (paddleW/2.0f);
        ball.vx = hitPos * 4.0f;
        PlaySound(TEXT("hit.wav"), NULL, SND_ASYNC);
    }
    for(int i=0;i<brickCount;i++){
        if(!bricks[i].alive) continue;
        if(ball.x > bricks[i].x && ball.x < bricks[i].x+bricks[i].w &&
           ball.y > bricks[i].y && ball.y < bricks[i].y+bricks[i].h){
            bricks[i].hp--;
            if(bricks[i].hp<=0){ bricks[i].alive=0; score+=10; spawnParticles(ball.x,ball.y); spawnPower(ball.x,ball.y); }
            if(!ball.throughBrick) ball.vy *= -1.0f;
            PlaySound(TEXT("hit.wav"), NULL, SND_ASYNC);
        }
    }
}

// ================= WIN CHECK =================
int allBricksDestroyed(){
    for(int i=0;i<brickCount;i++) if(bricks[i].alive) return 0;
    return 1;
}

// ================= PHYSICS =================
void updatePhysics(){
    if(leftDown) paddleX -= 4.0f;
    if(rightDown) paddleX += 4.0f;
    paddleX = clampf(paddleX,0.0f,(float)WINDOW_W - paddleW);

    if(!ballLaunched){ ball.x = paddleX + paddleW/2.0f; ball.y = paddleY + PADDLE_H + BALL_R; return; }

    ball.x += ball.vx; ball.y += ball.vy;

    if(ball.x - ball.r < 0){ ball.x=ball.r; ball.vx*=-1; }
    if(ball.x + ball.r > WINDOW_W){ ball.x=WINDOW_W-ball.r; ball.vx*=-1; }
    if(ball.y + ball.r > WINDOW_H){ ball.y=WINDOW_H-ball.r; ball.vy*=-1; }

    checkCollisions();
    updateParticles();
    updatePowers();
    updateBullets();

    if(allBricksDestroyed()){
        // advance to WIN state
        state = WIN;
        ball.active = 0;
        stopBackground(); // stop BG music for win screen
        return;
    }

    if(ball.y - ball.r < 0){
        lives--;
        if(lives>0) resetBall();
        else{
            state = GAMEOVER;
            if(score>highscore){ highscore = score; saveHighscore(); }
            stopBackground();
        }
    }
}

// ================= RENDERERS =================
void renderMenu(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    // Title
    glColor3f(1,1,1);
    drawString(WINDOW_W/2 - 80, WINDOW_H - 120, "DX-Ball (Fixed Menu)");
    // Menu Items
    const char* items[MENU_ITEMS] = {
        "Start New Game",
        "Resume",
        "High Score",
        "Help",
        "Exit"
    };
    for(int i=0;i<MENU_ITEMS;i++){
        if(i == menuSelection) glColor3f(1.0f,0.9f,0.2f);
        else glColor3f(1.0f,1.0f,1.0f);
        drawString(WINDOW_W/2 - 80, WINDOW_H/2 + 40 - i*30, items[i]);
    }
    glutSwapBuffers();
}
void renderHelp(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1,1,1);
    drawString(80, WINDOW_H/2 + 80, "HELP:");
    drawString(80, WINDOW_H/2 + 40, "Left/Right arrow or mouse to move.");
    drawString(80, WINDOW_H/2 + 20, "Click left mouse to launch ball.");
    drawString(80, WINDOW_H/2, "Space to shoot bullets. P to pause.");
    drawString(80, WINDOW_H/2 - 40, "Press ESC to return to menu.");
    glutSwapBuffers();
}

void renderScene(){
    glClear(GL_COLOR_BUFFER_BIT);

    if(state==MENU){ renderMenu(); return; }
    else if(state==HIGHSCORE){
        glColor3f(1,1,1);
        drawString(WINDOW_W/2-60, WINDOW_H/2+20,"HIGH SCORE");
        char buf[64]; sprintf(buf,"%d",highscore); drawString(WINDOW_W/2-30, WINDOW_H/2-10, buf);
        drawString(WINDOW_W/2-120, WINDOW_H/2-50,"Press ESC to go back");
        glutSwapBuffers();
        return;
    }
    else if(state==HELP){ renderHelp(); return; }
    else if(state==PLAYING || state==PAUSED){
        // Draw bricks
        for(int i=0;i<brickCount;i++){
            if(!bricks[i].alive) continue;
            switch(bricks[i].colorIdx){
                case 0: glColor3f(0.8,0.2,0.2); break;
                case 1: glColor3f(0.2,0.8,0.2); break;
                case 2: glColor3f(0.2,0.2,0.8); break;
                case 3: glColor3f(0.8,0.8,0.2); break;
                default: glColor3f(0.6,0.4,0.8);
            }
            drawRect(bricks[i].x, bricks[i].y, bricks[i].w, bricks[i].h);
        }

        // Draw paddle
        glColor3f(0.9,0.9,0.9);
        drawRect(paddleX,paddleY,paddleW,PADDLE_H);

        // Draw ball
        glColor3f(1.0, 0.6, 0.0);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(ball.x,ball.y);
        int slices=16;
        for(int j=0;j<=slices;j++){
            float a=j*2.0f*M_PI/slices;
            glVertex2f(ball.x+cosf(a)*ball.r, ball.y+sinf(a)*ball.r);
        }
        glEnd();

        // Draw bullets
        renderBullets();

        // Draw power-ups
        for(int i=0;i<powerCount;i++){
            if(!powers[i].alive) continue;
            switch(powers[i].type){
                case EXTRA_LIFE: glColor3f(0.9,0.4,0.4); break;
                case FASTER_BALL: glColor3f(0.4,0.9,0.4); break;
                case WIDER_PADDLE: glColor3f(0.4,0.4,0.9); break;
                case FIREBALL: glColor3f(1.0,0.5,1.0); break;
                case THROUGH_BRICK: glColor3f(0.5,1.0,1.0); break;
                case SHRINK_PADDLE: glColor3f(1.0,0.5,0.5); break;
                case IMMEDIATE_DEATH: glColor3f(0.8,0.1,0.1); break;
            }
            drawRect(powers[i].x-8, powers[i].y-8, 16,16);
        }

        // Draw particles
        glColor3f(1,1,0);
        for(int i=0;i<particleCount;i++){
            if(!particles[i].alive) continue;
            drawRect(particles[i].x,particles[i].y,particles[i].r,particles[i].r);
        }

        // Draw HUD
        glColor3f(1,1,1);
        char hud[128]; sprintf(hud,"Score: %d  Lives: %d  Level: %d",score,lives,level);
        drawString(20,WINDOW_H-20,hud);
        char hs[64]; sprintf(hs,"High: %d",highscore); drawString(WINDOW_W-120,WINDOW_H-20,hs);

        // Pause overlay
        if(state==PAUSED) drawString(WINDOW_W/2-40,WINDOW_H/2,"PAUSED");
        if(!ballLaunched) drawString(WINDOW_W/2-60,WINDOW_H/2-20,"Click Mouse to Launch Ball");

        glutSwapBuffers();
        return;
    }
    else if(state==WIN){
        glColor3f(1,1,1);
        drawString(WINDOW_W/2-60, WINDOW_H/2+20, "YOU WIN!");
        char buf[64]; sprintf(buf,"Score: %d  Level: %d", score, level);
        drawString(WINDOW_W/2-80, WINDOW_H/2, buf);
        drawString(WINDOW_W/2-120, WINDOW_H/2-40, "Press ENTER to go to next level or ESC to return to menu");
        glutSwapBuffers();
        return;
    }
    else if(state==GAMEOVER){
        glColor3f(1,1,1);
        drawString(WINDOW_W/2-60,WINDOW_H/2+20,"GAME OVER");
        char buf[64]; sprintf(buf,"Score: %d",score); drawString(WINDOW_W/2-60,WINDOW_H/2-10,buf);
        drawString(WINDOW_W/2-120,WINDOW_H/2-40,"Press ENTER to Restart or ESC to Exit");
        glutSwapBuffers();
        return;
    }

    glutSwapBuffers();
}

// ================= TIMER =================
void update(int value){
    if(state==PLAYING) updatePhysics();
    glutPostRedisplay();
    glutTimerFunc(10, update, 0);
}

// ================= INPUT =================
void keyDown(unsigned char key,int x,int y){
    if(state==MENU){
        if(key==13){ // ENTER
            if(menuSelection==0) startNewGame();
            else if(menuSelection==1) resumeGame();
            else if(menuSelection==2) state = HIGHSCORE;
            else if(menuSelection==3) state = HELP;
            else if(menuSelection==4) exit(0);
        }
        else if(key==27){ exit(0); }
    }
    else if(state==HIGHSCORE || state==HELP){
        if(key==27){ state=MENU; menuSelection=0; }
    }
    else if(state==PLAYING){
        if(key=='p'||key=='P'){ state=PAUSED; stopBackground(); }
        if(key==' '){ spawnBullet(paddleX + paddleW/2, paddleY + PADDLE_H); } // Shoot bullet
        if(key==27){ state=MENU; menuSelection=0; stopBackground(); }
    }
    else if(state==PAUSED){
        if(key=='p'||key=='P'){ state=PLAYING; startBackground(); }
        if(key==27){ state=MENU; menuSelection=0; stopBackground(); }
    }
    else if(state==GAMEOVER){
        if(key==13){ startNewGame(); }
        if(key==27) exit(0);
    }
    else if(state==WIN){
        if(key==13){
            // Advance to next level
            level++;
            clearArrays();
            initBricks();
            resetBall();
            state = PLAYING;
            startBackground();
        }
        if(key==27){
            // go back to menu
            state = MENU;
            menuSelection = 0;
        }
    }
}
void specialKeyDown(int key,int x,int y){
    if(key==GLUT_KEY_LEFT) leftDown=1;
    if(key==GLUT_KEY_RIGHT) rightDown=1;
    if(state==MENU){
        if(key==GLUT_KEY_UP){ menuSelection--; if(menuSelection<0) menuSelection=MENU_ITEMS-1; }
        if(key==GLUT_KEY_DOWN){ menuSelection++; if(menuSelection>=MENU_ITEMS) menuSelection=0; }
    }
}
void specialKeyUp(int key,int x,int y){ if(key==GLUT_KEY_LEFT) leftDown=0; if(key==GLUT_KEY_RIGHT) rightDown=0; }
void mouseMotion(int x,int y){
    paddleX = (float)x - paddleW/2.0f; paddleX = clampf(paddleX,0.0f,(float)WINDOW_W - paddleW);
}
void mouseClick(int button,int buttonState,int x,int y){
    if(state==PLAYING && !ballLaunched && button==GLUT_LEFT_BUTTON && buttonState==GLUT_DOWN){
        ball.vx = dx; ball.vy = dy; ballLaunched = 1;
        PlaySound(TEXT("launch.wav"), NULL, SND_ASYNC);
    }
}

// ================= MAIN =================
void reshape(int w,int h){
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0,w,0,h,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}
int main(int argc,char**argv){
    srand((unsigned int)time(NULL));
    loadHighscore();
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_W,WINDOW_H);
    glutCreateWindow("DX-Ball - OpenGL C (Fixed)");

    glClearColor(0.05f,0.05f,0.1f,1.0f);
    paddleX = WINDOW_W/2 - paddleW/2;

    // initialize arrays
    clearArrays();
    initBricks();
    resetBall();

    // Callbacks
    glutDisplayFunc(renderScene);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown);
    glutSpecialFunc(specialKeyDown);
    glutSpecialUpFunc(specialKeyUp);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(mouseMotion);
    glutMouseFunc(mouseClick);

    state=MENU; menuSelection=0;

    // NOTE: Do NOT play background music here. Start when gameplay begins.
    // PlaySound(TEXT("background.wav"), NULL, SND_ASYNC | SND_LOOP);

    glutTimerFunc(10, update, 0);
    glutMainLoop();
    return 0;
}
