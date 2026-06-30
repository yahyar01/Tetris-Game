#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h> 
#include <SDL3/SDL_ttf.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>   
#include <stdlib.h> 
#include <SDL3/SDL_main.h>
#include <stdio.h>

/* ===================== CONSTANTES ===================== */
#define WINDOW_W 900
#define WINDOW_H 600
#define CELL_SIZE 30           
#define GRID_COLS 10          
#define GRID_ROWS 20          
#define RIGHT_PANEL_WIDTH 200
#define LEFT_PANEL_WIDTH 200  
#define SIDE_MARGIN 40        
#define PREVIEW_CELL_SIZE 25  // Taille réduite pour les cellules HOLD/NEXT

/* ===================== STRUCTURES ===================== */
typedef struct {
    int forme[4][4];
    SDL_Color couleur;
} PieceDefinition;

typedef struct {
    int x, y;
    int forme[4][4];
    SDL_Color couleur;
    int type; // 0-6 pour identifier le type de pièce
} PieceActive;

typedef enum {
    STATE_START,
    STATE_PLAYING,
    STATE_PAUSE,
    STATE_GAMEOVER
} GameState;

/* ===================== VARIABLES GLOBALES ===================== */
GameState state = STATE_START;
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;
SDL_Texture* logo = NULL;
// Positions calculées automatiquement
int game_offset_x;  // Position X de la grille de jeu (centrée)
int game_offset_y;  // Position Y de la grille de jeu

// Grille de jeu (0 = vide, 1-7 = type de pièce)
int grille[GRID_ROWS][GRID_COLS] = {0};

// Pièces actives du jeu
PieceActive piece_courante;
PieceActive piece_suivante;
PieceActive piece_hold;
bool can_hold = true; // Permet d'utiliser hold une fois par pièce

// Variables de jeu
int score = 0;
int niveau = 1;
int lignes_completees = 0;
int vitesse_chute = 1000; // en ms
Uint64 dernier_temps_chute = 0;
bool game_started = false;

// Définitions des formes Tetris
PieceDefinition pieces_def[7];
SDL_Color couleurs_pieces[7] = {
    {255, 0, 0, 255},     // Rouge - O
    {0, 255, 0, 255},     // Vert - I
    {0, 0, 255, 255},     // Bleu - S
    {0, 255, 255, 255},   // Cyan - L
    {255, 0, 255, 255},   // Magenta - T
    {255, 165, 0, 255},   // Orange - J
    {255, 255, 0, 255},   // Jaune - Z
};

// Formes des pièces Tetris (version centrée pour hold/suivant)
int formes[7][4][4] = {
    // O (carré)
    {{0,0,0,0},
     {0,1,1,0},
     {0,1,1,0},
     {0,0,0,0}},
    // I (bâton)
    {{0,0,0,0},
     {1,1,1,1},
     {0,0,0,0},
     {0,0,0,0}},
    // S
    {{0,0,0,0},
     {0,0,1,1},
     {0,1,1,0},
     {0,0,0,0}},
    // L
    {{0,0,0,0},
     {0,1,1,1},
     {0,1,0,0},
     {0,0,0,0}},
    // T
    {{0,0,0,0},
     {0,1,1,1},
     {0,0,1,0},
     {0,0,0,0}},
    // J
    {{0,0,0,0},
     {0,1,1,1},
     {0,0,0,1},
     {0,0,0,0}},
    // Z
    {{0,0,0,0},
     {1,1,0,0},
     {0,1,1,0},
     {0,0,0,0}}
};

/* ===================== PARTIE 1: MOTEUR DU JEU ET SCORE ===================== */
void initialiser_pieces_definitions() {
    for(int i = 0; i < 7; i++) {
        memcpy(pieces_def[i].forme, formes[i], sizeof(formes[i]));
        pieces_def[i].couleur = couleurs_pieces[i];
    }
}

PieceActive creer_piece_aleatoire() {
    PieceActive nouvelle_piece;
    int type = rand() % 7;
    
    nouvelle_piece.type = type;
    nouvelle_piece.couleur = couleurs_pieces[type];
    memcpy(nouvelle_piece.forme, formes[type], sizeof(formes[type]));
    
    // Position initiale (centrée en haut)
    nouvelle_piece.x = GRID_COLS / 2 - 2;
    nouvelle_piece.y = 0;
    
    return nouvelle_piece;
}

bool collision(PieceActive piece) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(piece.forme[i][j] == 1) {
                int grille_x = piece.x + j;
                int grille_y = piece.y + i;
                
                // Vérifier les bords
                if(grille_x < 0 || grille_x >= GRID_COLS || grille_y >= GRID_ROWS) {
                    return true;
                }
                
                // Vérifier collision avec autres pièces
                if(grille_y >= 0 && grille[grille_y][grille_x] != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

void fixer_piece() {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(piece_courante.forme[i][j] == 1) {
                int grille_x = piece_courante.x + j;
                int grille_y = piece_courante.y + i;
                
                if(grille_y >= 0 && grille_y < GRID_ROWS && grille_x >= 0 && grille_x < GRID_COLS) {
                    grille[grille_y][grille_x] = piece_courante.type + 1; // 1-7
                }
            }
        }
    }
    
    // Réinitialiser le hold
    can_hold = true;
}

void rotation_90_degrees(PieceActive *piece) {
    int temp[4][4];
    
    // Rotation à 90 degrés dans le sens horaire
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            temp[j][3-i] = piece->forme[i][j];
        }
    }
    
    // Sauvegarder l'ancienne rotation
    int ancienne_forme[4][4];
    memcpy(ancienne_forme, piece->forme, sizeof(piece->forme));
    
    // Tester la nouvelle rotation
    memcpy(piece->forme, temp, sizeof(temp));
    
    // Si collision, annuler la rotation
    if(collision(*piece)) {
        memcpy(piece->forme, ancienne_forme, sizeof(ancienne_forme));
    }
}
/* ===================== DÉPLACEMENT ===================== */

void deplacer_piece(int dx, int dy) {
    PieceActive test_piece = piece_courante;
    test_piece.x += dx;
    test_piece.y += dy;
    
    if(!collision(test_piece)) {
        piece_courante.x += dx;
        piece_courante.y += dy;
    }
    else if(dy > 0) { // Si on ne peut pas descendre, fixer la pièce
        fixer_piece();
        
        // Vérifier et supprimer les lignes complètes
        int lignes_supprimees = 0;
        for(int ligne = GRID_ROWS - 1; ligne >= 0; ligne--) {
            int ligne_complete = 1;
            
            for(int col = 0; col < GRID_COLS; col++) {
                if(grille[ligne][col] == 0) {
                    ligne_complete = 0;
                    break;
                }
            }
            
            if(ligne_complete) {
                lignes_supprimees++;
                
                // Descendre toutes les lignes au-dessus
                for(int l = ligne; l > 0; l--) {
                    for(int c = 0; c < GRID_COLS; c++) {
                        grille[l][c] = grille[l-1][c];
                    }
                }
                
                // Remplir la première ligne avec des zéros
                for(int c = 0; c < GRID_COLS; c++) {
                    grille[0][c] = 0;
                }
                
                // Vérifier à nouveau la même ligne
                ligne++;
            }
        }
        
        // Mettre à jour le score si des lignes ont été supprimées
        if(lignes_supprimees > 0) {
            switch(lignes_supprimees) {
                case 1: score += 100 * niveau; break;
                case 2: score += 300 * niveau; break;
                case 3: score += 500 * niveau; break;
                case 4: score += 800 * niveau; break; // Tetris
            }
            
            lignes_completees += lignes_supprimees;
            
            // Bonus spécial pour Tetris
            if(lignes_supprimees == 4) {
                printf("TETRIS! Bonus spécial!\n");
            }
            
            // Augmente le niveau tous les 10 lignes
            int nouveau_niveau = (lignes_completees / 10) + 1;
            if(nouveau_niveau > niveau) {
                niveau = nouveau_niveau;
                vitesse_chute = 1000 - (niveau - 1) * 100; // Diminue le temps de chute
                if(vitesse_chute < 100) vitesse_chute = 100; // Vitesse minimum
                printf("Niveau %d atteint! Vitesse: %dms\n", niveau, vitesse_chute);
            }
        }
        
        // Passer à la pièce suivante
        piece_courante = piece_suivante;
        piece_suivante = creer_piece_aleatoire();
        
        // Vérifier game over
        if(collision(piece_courante)) {
            state = STATE_GAMEOVER;
        }
    }
}

void hard_drop() {
    while(!collision(piece_courante)) {
        piece_courante.y++;
    }
    piece_courante.y--; // Remonter d'un cran
    fixer_piece();
    
    // Vérifier et supprimer les lignes complètes
    int lignes_supprimees = 0;
    for(int ligne = GRID_ROWS - 1; ligne >= 0; ligne--) {
        int ligne_complete = 1;
        
        for(int col = 0; col < GRID_COLS; col++) {
            if(grille[ligne][col] == 0) {
                ligne_complete = 0;
                break;
            }
        }
        
        if(ligne_complete) {
            lignes_supprimees++;
            
            for(int l = ligne; l > 0; l--) {
                for(int c = 0; c < GRID_COLS; c++) {
                    grille[l][c] = grille[l-1][c];
                }
            }
            
            for(int c = 0; c < GRID_COLS; c++) {
                grille[0][c] = 0;
            }
            
            ligne++;
        }
    }
    
    // Mettre à jour le score
    if(lignes_supprimees > 0) {
        switch(lignes_supprimees) {
            case 1: score += 100 * niveau; break;
            case 2: score += 300 * niveau; break;
            case 3: score += 500 * niveau; break;
            case 4: score += 800 * niveau; break;
        }
        
        lignes_completees += lignes_supprimees;
        
        if(lignes_supprimees == 4) {
            printf("TETRIS! Bonus spécial!\n");
        }
        
        int nouveau_niveau = (lignes_completees / 10) + 1;
        if(nouveau_niveau > niveau) {
            niveau = nouveau_niveau;
            vitesse_chute = 1000 - (niveau - 1) * 100;
            if(vitesse_chute < 100) vitesse_chute = 100;
            printf("Niveau %d atteint! Vitesse: %dms\n", niveau, vitesse_chute);
        }
    }
    
    // Nouvelle pièce
    piece_courante = piece_suivante;
    piece_suivante = creer_piece_aleatoire();
    
    if(collision(piece_courante)) {
        state = STATE_GAMEOVER;
    }
}

/* ===================== SYSTÈME HOLD FONCTIONNEL ===================== */
void utiliser_hold() {
    if(!can_hold) return;
    
    // Si pas de pièce en hold, mettre la pièce courante en hold
    if(piece_hold.type == -1) {
        piece_hold = piece_courante;
        // Réinitialiser la position pour le hold
        piece_hold.x = 0;
        piece_hold.y = 0;
        
        // Prendre la pièce suivante comme nouvelle pièce courante
        piece_courante = piece_suivante;
        piece_suivante = creer_piece_aleatoire();
    }
    else {
        // Échanger la pièce courante avec celle en hold
        PieceActive temp = piece_courante;
        
        // Prendre la pièce du hold comme nouvelle pièce courante
        piece_courante = piece_hold;
        piece_courante.x = GRID_COLS / 2 - 2; // Position de départ
        piece_courante.y = 0;
        
        // Mettre l'ancienne pièce courante dans le hold
        piece_hold = temp;
        piece_hold.x = 0;
        piece_hold.y = 0;
    }
    
    // Désactiver le hold jusqu'à la prochaine pièce
    can_hold = false;
    
    // Vérifier si la nouvelle pièce cause un game over immédiat
    if(collision(piece_courante)) {
        state = STATE_GAMEOVER;
    }
}

void reinitialiser_score() {
    score = 0;
    niveau = 1;
    lignes_completees = 0;
    vitesse_chute = 1000;
    printf("Score réinitialisé. Nouvelle partie!\n");
}

void initialiser_jeu() {
    // Réinitialiser la grille
    for(int i = 0; i < GRID_ROWS; i++) {
        for(int j = 0; j < GRID_COLS; j++) {
            grille[i][j] = 0;
        }
    }
    
    // Réinitialiser le score
    reinitialiser_score();
    
    // Initialiser les pièces
    piece_courante = creer_piece_aleatoire();
    piece_suivante = creer_piece_aleatoire();
    piece_hold.type = -1; // Aucune pièce en hold (-1 = vide)
    
    // Réinitialiser le temps
    dernier_temps_chute = SDL_GetTicks();
    can_hold = true;
    
    game_started = true;
    
    printf("Jeu initialisé! Prêt à jouer.\n");
}

/* ===================== PARTIE 2: FONCTIONS DESSIN ===================== */
void dessiner_piece_active(SDL_Renderer *renderer, PieceActive *piece, int offset_x, int offset_y) {
    SDL_SetRenderDrawColor(renderer, 
        piece->couleur.r, 
        piece->couleur.g, 
        piece->couleur.b, 
        piece->couleur.a 
    );
    
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(piece->forme[i][j] == 1) {
                float x_pixel = offset_x + (piece->x + j) * CELL_SIZE; 
                float y_pixel = offset_y + (piece->y + i) * CELL_SIZE;
                SDL_FRect bloc = {x_pixel + 1, y_pixel + 1, CELL_SIZE - 2, CELL_SIZE - 2};
                SDL_RenderFillRect(renderer, &bloc);
            }
        }
    }
}

void dessiner_grille_jeu(SDL_Renderer *renderer, int offset_x, int offset_y) {
    // Dessiner les blocs de la grille
    for(int i = 0; i < GRID_ROWS; i++) {
        for(int j = 0; j < GRID_COLS; j++) {
            if(grille[i][j] != 0) {
                int type = grille[i][j] - 1;
                SDL_SetRenderDrawColor(renderer, 
                    couleurs_pieces[type].r,
                    couleurs_pieces[type].g,
                    couleurs_pieces[type].b,
                    255
                );
                SDL_FRect bloc = {
                    offset_x + j * CELL_SIZE + 1,
                    offset_y + i * CELL_SIZE + 1,
                    CELL_SIZE - 2,
                    CELL_SIZE - 2
                };
                SDL_RenderFillRect(renderer, &bloc);
            }
        }
    }
    
    // Dessiner la grille (lignes)
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    
    // Lignes verticales
    for(int j = 0; j <= GRID_COLS; j++) {
        float x = offset_x + j * CELL_SIZE;
        SDL_RenderLine(renderer, x, offset_y, x, offset_y + GRID_ROWS * CELL_SIZE);
    }
    
    // Lignes horizontales
    for(int i = 0; i <= GRID_ROWS; i++) {
        float y = offset_y + i * CELL_SIZE;
        SDL_RenderLine(renderer, offset_x, y, offset_x + GRID_COLS * CELL_SIZE, y);
    }
}

void dessiner_piece_preview(SDL_Renderer *renderer, PieceActive *piece, int cadre_x, int cadre_y, int cadre_width, int cadre_height) {
    if(piece->type == -1) return; // Pas de pièce à afficher
    
    // Calculer les dimensions de la pièce pour le centrage
    int min_x = 4, max_x = -1, min_y = 4, max_y = -1;
    
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(piece->forme[i][j] == 1) {
                if(j < min_x) min_x = j;
                if(j > max_x) max_x = j;
                if(i < min_y) min_y = i;
                if(i > max_y) max_y = i;
            }
        }
    }
    
    if(max_x == -1 || max_y == -1) return;
    
    int piece_width = (max_x - min_x + 1) * PREVIEW_CELL_SIZE;
    int piece_height = (max_y - min_y + 1) * PREVIEW_CELL_SIZE;
    
    // Centrer dans le cadre
    int offset_x = cadre_x + (cadre_width - piece_width) / 2;
    int offset_y = cadre_y + (cadre_height - piece_height) / 2;
    
    SDL_SetRenderDrawColor(renderer, 
        piece->couleur.r, 
        piece->couleur.g, 
        piece->couleur.b, 
        piece->couleur.a 
    );
    
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(piece->forme[i][j] == 1) {
                float x_pixel = offset_x + (j - min_x) * PREVIEW_CELL_SIZE; 
                float y_pixel = offset_y + (i - min_y) * PREVIEW_CELL_SIZE;
                SDL_FRect bloc = {x_pixel + 1, y_pixel + 1, PREVIEW_CELL_SIZE - 2, PREVIEW_CELL_SIZE - 2};
                SDL_RenderFillRect(renderer, &bloc);
            }
        }
    }
}

/* ===================== PARTIE 3: FONCTIONS UTILITAIRES ===================== */
void draw_text(const char* txt, int x, int y,int w, SDL_Color c) {
    SDL_Surface* s = TTF_RenderText_Solid(font, txt, strlen(txt), c);
    if (!s) {
        printf("Erreur création surface texte: %s\n", SDL_GetError());
        return;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    if (!t) {
        printf("Erreur création texture texte: %s\n", SDL_GetError());
        SDL_DestroySurface(s);
        return;
    }
    SDL_FRect r = {x + (w - s->w) / 2, y, s->w, s->h};
    SDL_RenderTexture(renderer, t, NULL, &r);
    SDL_DestroyTexture(t);
    SDL_DestroySurface(s);
}

bool button(const char* txt, int x, int y, int w, int h) {
    SDL_FRect r = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    SDL_RenderRect(renderer, &r);
    draw_text(txt, x, y + 10, w, (SDL_Color){255, 255, 255, 255});

    float mx, my;
    SDL_MouseButtonFlags m = SDL_GetMouseState(&mx, &my);
    return (m & SDL_BUTTON_LEFT) && mx > x && mx < x + w && my > y && my < y + h;
}

/* ===================== PARTIE 4: ÉCRANS ===================== */
void draw_start() {
    if (logo) {
        SDL_FRect r = {300, 80, 300, 120};
        SDL_RenderTexture(renderer, logo, NULL, &r);
    }
    
    if (button("COMMENCER", 380, 250, 140, 50)) {
        initialiser_jeu();
        state = STATE_PLAYING;
    }
    if (button("  QUITTER  ",380, 320, 140, 50)) {
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        exit(0);
    }
    draw_text("Membres du groupe:", 220, WINDOW_H - 100, WINDOW_W/2, (SDL_Color){180,180,180,255});
    draw_text("Rami Yahya, Darbouti Ayoub, ELHamdaoui Mohammed, Rghioui Rachid, Dhoura Ismail", 180, WINDOW_H - 70, WINDOW_W/2, (SDL_Color){180,180,180,255});
  
}

void draw_pause() {
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_FRect overlay = {0, 0, WINDOW_W, WINDOW_H};
    SDL_RenderFillRect(renderer, &overlay);
    
    draw_text("PAUSE", WINDOW_W/2 - 100, 120, 200, (SDL_Color){255, 255, 255, 255});

    if (button("CONTINUER", 360, 220, 190, 40))
        state = STATE_PLAYING;

    if (button("RECOMMENCER", 360, 270, 190, 40)) {
        initialiser_jeu();
        state = STATE_PLAYING;
    }

    if (button("QUITTER", 360, 320, 190, 40)) {
        if (font) TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        exit(0);
    }
}

void draw_gameover() {
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    draw_text("GAME OVER", WINDOW_W/2 - 100, 120, 200, (SDL_Color){255, 0, 0, 255});
    

    char score_text[50];
    sprintf(score_text, "Score final: %d", score);
    draw_text(score_text, WINDOW_W/2 - 100, 200, 200, (SDL_Color){255, 255, 255, 255});
    
    char niveau_text[50];
    sprintf(niveau_text, "Niveau atteint: %d", niveau);
    draw_text(niveau_text, WINDOW_W/2 - 100, 230, 200, (SDL_Color){255, 255, 255, 255});
    
    char lignes_text[50];
    sprintf(lignes_text, "Lignes complétées: %d", lignes_completees);
    draw_text(lignes_text, WINDOW_W/2 - 100, 260, 200, (SDL_Color){255, 255, 255, 255});
    if (button("REJOUER", 370, 320, 160, 40)) {
        initialiser_jeu();
        state = STATE_PLAYING;
    }

    if (button("QUITTER", 370, 370, 160, 40)) {
        if (font) TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        exit(0);
    }
}

void draw_playing() {
    int left_panel_x = 0;
    int right_panel_x = WINDOW_W - RIGHT_PANEL_WIDTH;
    
    // Espace disponible pour la grille
    int available_width = WINDOW_W - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
    game_offset_x = LEFT_PANEL_WIDTH + (available_width - GRID_COLS * CELL_SIZE) / 2;
    game_offset_y = (WINDOW_H - GRID_ROWS * CELL_SIZE) / 2;
    
    // Panneau gauche (HOLD)
    SDL_FRect left_side = {left_panel_x, 0, LEFT_PANEL_WIDTH, WINDOW_H};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &left_side);
    
    // Titre HOLD
    int left_panel_content_x = left_panel_x + SIDE_MARGIN;
    draw_text("HOLD", left_panel_content_x + 40, 40, LEFT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});
    
    // Zone d'affichage de la pièce HOLD
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    int hold_box_x = left_panel_content_x + 20;
    int hold_box_y = 80;
    int hold_box_width = 120;
    int hold_box_height = 120;
    SDL_FRect hold_box = {hold_box_x, hold_box_y, hold_box_width, hold_box_height};
    SDL_RenderRect(renderer, &hold_box);
    
    // Afficher la pièce en hold (si elle existe)
    if(piece_hold.type != -1) {
        dessiner_piece_preview(renderer, &piece_hold, hold_box_x, hold_box_y, hold_box_width, hold_box_height);
    }
    
    // Panneau droit
    SDL_FRect right_side = {(float)right_panel_x, 0, RIGHT_PANEL_WIDTH, (float)WINDOW_H};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(renderer, &right_side);

    // Informations dans le panneau droit
    int panel_content_x = right_panel_x + SIDE_MARGIN;
    
    // Score
    draw_text("SCORE", panel_content_x + 40, 40, RIGHT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});
    char score_str[20];
    sprintf(score_str, "%d", score);
    draw_text(score_str, panel_content_x + 40, 70, RIGHT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});

    // Niveau
    draw_text("NIVEAU", panel_content_x + 40, 120,RIGHT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});
    char niveau_str[20];
    sprintf(niveau_str, "%d", niveau);
    draw_text(niveau_str, panel_content_x + 40, 150, RIGHT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});

    // Lignes
    draw_text("LIGNES", panel_content_x + 40, 200,RIGHT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});
    char lignes_str[20];
    sprintf(lignes_str, "%d", lignes_completees);
    draw_text(lignes_str, panel_content_x + 40, 230, RIGHT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});

    // Pièce suivante
    draw_text("SUIVANTE", panel_content_x + 30, 280, RIGHT_PANEL_WIDTH/2, (SDL_Color){255, 255, 255, 255});
    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
    int next_box_x = panel_content_x + 20;
    int next_box_y = 310;
    int next_box_width = 120;
    int next_box_height = 120;
    SDL_FRect next_box = {next_box_x, next_box_y, next_box_width, next_box_height};
    SDL_RenderRect(renderer, &next_box);
    
    // Afficher la pièce suivante
    dessiner_piece_preview(renderer, &piece_suivante, next_box_x, next_box_y, next_box_width, next_box_height);

    // Bouton Menu
    if (button("MENU", panel_content_x + 20, 500, 120, 40))
        state = STATE_PAUSE;

    // Zone de jeu principale
    SDL_SetRenderDrawColor(renderer, 10, 10, 30, 255);
    SDL_FRect play_area = {(float)game_offset_x, (float)game_offset_y, 
                          (float)(GRID_COLS * CELL_SIZE), 
                          (float)(GRID_ROWS * CELL_SIZE)};
    SDL_RenderFillRect(renderer, &play_area);

    // Afficher la grille et les pièces
    dessiner_grille_jeu(renderer, game_offset_x, game_offset_y);
    dessiner_piece_active(renderer, &piece_courante, game_offset_x, game_offset_y);
}

/* ===================== PARTIE 5: CONTRÔLES ===================== */
void handleInput(SDL_Event *event) {
    if (event->type != SDL_EVENT_KEY_DOWN)
        return;
    
    switch (event->key.scancode) {
        case SDL_SCANCODE_LEFT:
            deplacer_piece(-1, 0);
            break;
        case SDL_SCANCODE_RIGHT:
            deplacer_piece(1, 0);
            break;
        case SDL_SCANCODE_DOWN:
            deplacer_piece(0, 1);
            break;
        case SDL_SCANCODE_UP:
            rotation_90_degrees(&piece_courante);
            break;
        case SDL_SCANCODE_SPACE:
            hard_drop();
            break;
        case SDL_SCANCODE_C:
            utiliser_hold();
            break;
    }
}

/* ===================== MAIN ===================== */
int main(int argc, char* argv[]) {
    // Initialisation SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Erreur SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    
    if (TTF_Init() < 0) {
        printf("Erreur TTF_Init: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Initialiser le générateur aléatoire
    srand(time(NULL));
    
    // Création fenêtre
    window = SDL_CreateWindow("Tetris SDL3", WINDOW_W, WINDOW_H, 0);
    if (!window) {
        printf("Erreur création fenêtre: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Création renderer
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Erreur création renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    //charger l'image du logo
    logo = IMG_LoadTexture(renderer, "tetris.png");
    if (!logo) {
        SDL_Log("IMG_LoadTexture error: %s", SDL_GetError());
    }
    
    
    
    // Charger la police
    font = TTF_OpenFont("arial.ttf", 18);
    if (!font) {
        // Essayer d'autres chemins de polices
        font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    
    }
    
    if (!font) {
        printf("Erreur de chargement de la police\n");
        return 1;
    }
    // Initialiser les définitions de pièces
    initialiser_pieces_definitions();
    
    // Initialiser le jeu
    initialiser_jeu();
    
    // Boucle principale
    bool running = true;
    SDL_Event e;
    Uint64 temps_actuel;
    
    while (running) {
        temps_actuel = SDL_GetTicks();
        
        // Gestion des événements
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (state) {
                    case STATE_PLAYING:
                        if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_P) {
                            state = STATE_PAUSE;
                        } else {
                            handleInput(&e);
                        }
                        break;
                    case STATE_PAUSE:
                        if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_P) {
                            state = STATE_PLAYING;
                        }
                        break;
                    case STATE_START:
                        if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) {
                            initialiser_jeu();
                            state = STATE_PLAYING;
                        }
                        break;
                    case STATE_GAMEOVER:
                        if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) {
                            initialiser_jeu();
                            state = STATE_PLAYING;
                        }
                        break;
                }
            }
        }
        
        // Logique du jeu (uniquement en mode PLAYING)
        if (state == STATE_PLAYING && game_started) {
            // Chute automatique
            if (temps_actuel - dernier_temps_chute > vitesse_chute) {
                deplacer_piece(0, 1);
                dernier_temps_chute = temps_actuel;
            }
        }
        
        // Effacer l'écran
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Dessiner l'état courant
        switch (state) {
            case STATE_START: 
                draw_start(); 
                break;
            case STATE_PLAYING: 
                draw_playing(); 
                break;
            case STATE_PAUSE: 
                draw_pause(); 
                break;
            case STATE_GAMEOVER: 
                draw_gameover(); 
                break;
        }
        
        // Afficher
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    // Nettoyage
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}