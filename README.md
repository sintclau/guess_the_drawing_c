# Proiect: Guess the Drawing (ASCII Multiplayer Game)

## Descriere
Acest proiect este un joc multiplayer ASCII realizat în C, folosind `ncurses` pentru interfața grafică și `sockets` pentru comunicarea între server și clienți. Jocul presupune că un jucător desenează un cuvânt ales de server, iar ceilalți încearcă să ghicească.

## Tehnologii și Biblioteci Utilizate
- **Limbaj de programare:** C
- **Biblioteci principale:**
  - `ncurses` (pentru interfața ASCII)
  - `sys/socket.h`, `arpa/inet.h` (pentru rețea)
  - `pthread` (pentru gestionarea conexiunilor multiple)
- **Model client-server:** TCP/IP, cu un server central găzduit pe un VPS

## Structura Proiectului
1. **Server (`server.c`)**
   - Gestionează conexiunile multiple folosind `select()`
   - Alege un cuvânt dintr-o listă predefinită și îl trimite unui jucător pentru desen
   - Trimite actualizările desenului către ceilalți jucători
   - Primește răspunsurile de la jucători și verifică dacă cineva a ghicit corect

2. **Client (`client.c`)**
   - Se conectează la server
   - Un jucător va primi un cuvânt și va folosi `ncurses` pentru a desena
   - Ceilalți jucători vor vedea desenul în timp real și vor introduce răspunsuri
   - Trimite răspunsurile la server pentru verificare

## Pași pentru Implementare
1. **Realizarea conexiunii server-client**
2. **Adăugarea interfeței grafice cu `ncurses`**
3. **Implementarea logicii de alegere a cuvintelor și transmiterea acestora**
4. **Gestionarea sesiunii de desen și trimiterea coordonatelor**
5. **Verificarea răspunsurilor și gestionarea scorurilor**
6. **Îmbunătățiri și optimizări**

## Cum Se Rulează
1. Se compilează serverul:
   ```bash
   gcc server.c -o server
   ```
2. Se rulează serverul:
   ```bash
   ./server
   ```
3. Se compilează clientul:
   ```bash
   gcc client.c -o client -lncurses
   ```
4. Se rulează clientul:
   ```bash
   ./client
   ```
