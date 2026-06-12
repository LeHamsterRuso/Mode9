//------------------------------------------------------------------------------
// TITLE:       MODE9_05.PRG
// AUTHOR:      Sebastian J. MONCHO MAQUET
// DATE:        11/06/2026
// DESCRIPTION: Ejemplo del "modo 9" que INTERCAMBIA MAPAS en caliente. Parte del
//              MODE9_04.PRG (patio del castillo con camara primera persona):
//                - Al acercarse a la PUERTA DEL TORREON se ENTRA al castillo:
//                  se descarga el exterior (CASTLE.OBJ) y se carga el INTERIOR
//                  (CASTIN.OBJ), una sala cerrada con su propia textura.
//                - El NPC que en el MODE9_04 estaba en el patio AHORA esta DENTRO
//                  de la sala (se "muda" de mapa).
//                - Dentro, una PUERTA DE SALIDA devuelve al patio.
//
//              Objetivo: ver como se comporta el modo 9 al CAMBIAR de escenario
//              .OBJ y cargar/descargar assets (malla, texturas, cielo, sprites).
//
//              IMPORTANTE (por que TODO va inline en el proceso camara): en DIV2,
//              llamar a una FUNCTION/PROCESS referenciada antes de definirse NO
//              ejecuta su cuerpo de forma sincrona como cabria esperar; por eso el
//              cambio de mapa (signal + LOAD_WLD9 + recolocar la camara) se hace
//              LITERALMENTE dentro del bucle, sin llamar a ninguna funcion. Asi
//              LOAD_WLD9 se ejecuta de verdad y el mapa se recarga.
//
//              - RECARGA EN CALIENTE sin parar el modo 9: LOAD_WLD9 no toca el
//                estado del modo 9 (sigue ACTIVO) ni el enlace de la camara
//                (mismo id + direccion &m9.z), solo cambia la malla/materiales y
//                reconstruye colisiones. NO se llama a STOP_MODE9/START_MODE9.
//              - Tras cada cambio hay un COOLDOWN que impide re-disparar la puerta
//                hasta soltarla (evita rebotes mapa-a-mapa).
//              - LOAD_WLD9 es bloqueante: ignore_error(142) evita que el watchdog
//                ("proceso bloqueado, esperando FRAME") aborte la carga.
//
// CONTROLES:
//   W / S : avanzar / retroceder (SHIFT correr)   A / D : strafe
//   Raton : girar / mirar     ESPACIO : saltar     CTRL : agacharse    ESC : salir
//
// FICHEROS:
//   CASTLE.OBJ / CASTIN.OBJ        escenarios 3D (exterior / interior)
//   MAP/CASTLE.MAP STAIRS.MAP WATER.MAP   texturas del exterior (mats 0,1,2)
//   MAP/CASTIN_T.MAP                    textura del interior (mat 0; de CASTIN_T.png)
//   MAP/SKY.MAP                       cielo (solo exterior)
//   FPG/MODE9.FPG                     billboards (NPC, arbol, cartel)
//------------------------------------------------------------------------------
PROGRAM MODE9_05_DEMO;

IMPORT "MODE9.DLL";

CONST
    SCREEN_X   = 320;     // Ancho de la resolucion de la pantalla
    SCREEN_Y   = 200;     // Alto de la resolucion de la pantalla
    GAME_FPS   = 60;      // FPS objetivo
    ADV_UNIT   = 30;      // velocidad de paso (uds DIV2)
    c_m9       = 200;     // c_type "logico" del modo 9 (convencion, ver cabecera)

    MAP_OUT  = 0;          // exterior (patio)
    MAP_IN   = 1;          // interior (sala)
    SWAP_CD  = 40;         // frames de cooldown tras un cambio de mapa

    // Puerta del torreon (exterior): OBJ z=-4 -> mundo y=-400.
    DOOR_X = 0;   DOOR_Y = -400;   DOOR_R2 = 20000;   // radio de disparo ~141
    // Puerta de salida (interior): OBJ z=-6 -> mundo y=-600.
    EXIT_X = 0;   EXIT_Y = -600;   EXIT_R2 = 22500;   // radio de disparo ~150
    // NPC dentro de la sala (mismo sitio que NPC(0,500,50)): globo de dialogo al acercarse.
    NPC_X = 0;    NPC_Y = 500;     BUBBLE_R2 = 160000;   // radio del globo ~400
    NPC_HEAD = 220;       // punto (mundo) a la altura de la CABEZA: ancla del bocadillo y destino del rabito
    NPC_TOP  = 320;       // punto (mundo) SOBRE la cabeza: ahi va el nombre
    BUB_HW = 78;          // semiancho del bocadillo lateral (1 linea de frase)
    BUB_HH = 12;          // semialto del bocadillo
    BUB_GAP = 12;         // separacion entre el NPC y el bocadillo

GLOBAL
    STRUCT m9            // STRUCT de camara del modo 9 (mismos campos que m8). &m9.z = base del struct.
        z;
        camera;
        height;
        angle;
    END
    scr_x;                // resolucion real (la lee la DLL): raton/camara a
    scr_y;                // cualquier resolucion, no solo 320x200
    map_state;            // MAP_OUT / MAP_IN (se muestra como 1 digito arriba-izda)

BEGIN
    // Es probable que los assets tarden en cargar en hardware real.
    // Si te salta la expcion 142, puedes decomentar esta linea para ignorarla.
    //ignore_error(142);

    set_mode(SCREEN_X * 1000 + SCREEN_Y);
    set_fps(GAME_FPS, 0);

    scr_x = MODE9_SCRW();   // MODE9_SCRW() y MODE9_SCRH() se pueden llamar en cualquier momento, no solo al inicio, por si quieres cambiar la resolucion con set_mode durante el juego.
    scr_y = MODE9_SCRH();

    LOAD_FPG("FPG/MODE9.FPG");
    MODE9_LOADFPG("FPG/MODE9.FPG");

    MODE9_STEP(70);

    map_state = MAP_OUT;
    camara();           // muestra la pantalla de inicio, carga el 3D y juega
END


//------------------------------------------------------------------------------
// CAMARA: movimiento (igual que MODE9_04) + INTERCAMBIO DE MAPAS (todo inline).
//------------------------------------------------------------------------------
PROCESS camara()
PRIVATE
    mouse_x_prev;
    eye_base;
    fall_v;        // velocidad de caida (la gravedad la integra el PRG)
    grounded;      // 1 si la DLL nos sostiene en un suelo/escalon/rampa
    z_fell;        // z tras aplicar la gravedad, ANTES de que la DLL ajuste
    cooldown;          // frames restantes sin poder disparar puertas
    dx; dy; d2;
    bub_on;            // 1 = globo de dialogo del NPC visible
    bub_fill; bub_brd; bub_tail;   // bocadillo: elipse rellena, borde, rabito
    bub_spk;           // texto de la frase (dentro del bocadillo)
    name_plate; name_txt;          // placa + nombre, encima del NPC
    bd2;               // distancia (al cuadrado) de la camara al NPC
    show;              // 1 = se cumplen las condiciones para ver el globo
    nsx; nsy; nsz;     // proyeccion de la CABEZA del NPC (MODE9_PROJECT)
    tsx; tsy;          // proyeccion del punto SOBRE la cabeza (nombre)
    cx; cy; tx;        // centro del bocadillo y x del rabito en su borde
    pk;                // id del texto "Pulsa una tecla" (parpadeo)
    i; detected;       // sondeo de teclas en la pantalla de inicio
    sel; ed_cur;       // dialogo de salida: opcion elegida (1=Si,0=No) y cursor
BEGIN

    // =================== PANTALLA DE INICIO (pulsa una tecla) ===============
    // Aun no se ha cargado el mundo 3D: es una pantalla 2D normal.
    draw(3,  0, 3, 0,  18, 36, scr_x-18, scr_y-36);         // fondo oscuro
    draw(2, 15, 15, 0, 18, 36, scr_x-18, scr_y-36);         // marco
    write(0, scr_x/2, 56, 4, "EL CASTILLO 3D");
    write(0, scr_x/2, 74, 4, "( Modo 9 - DIV2 )");
    pk = 0;
    detected = 0;

    // Fondo del titulo (graph 14 = TITLEM9.PCX, 320x200).
    put_screen(0, 14);
    frame;
    // Esperar a que se pulse cualquier tecla. Se sondea key() de todos los
    // scancodes (estado real "pulsada"), porque scan_code aqui es un pulso de
    // 1 frame y se pierde si el sondeo no cae justo en ese frame.
    repeat
        if ((timer[0]/40) % 2 == 0)
            if (pk == 0)
                pk = write(0, scr_x/2, 138, 4, "Pulsa una tecla");
            end
        else
            if (pk <> 0)
                delete_text(pk);
                pk = 0;
            end
        end
        FRAME;
    until(scan_code == 0);
    // Esperar a soltar todas las teclas antes de empezar.
    repeat
        FRAME;
    until (scan_code != 0);
    delete_text(all_text);
    delete_draw(all_drawing);
    clear_screen();
    FRAME;
    // ---------------- CARGA INICIAL: EXTERIOR ----------------
    LOAD_WLD9("OBJ/CASTLE.OBJ", "MAP/CASTLE.MAP", 100);
    MODE9_MATERIAL(1, "MAP/STAIRS.MAP");
    MODE9_MATERIAL(2, "MAP/WATER.MAP");
    LOAD_SKY9("MAP/SKY.MAP");

    M9TREE(600, -1300, 0);          // props del patio (el NPC vive DENTRO, no aqui)
    M9TREE(-600, -1300, 0);
    M9TREE(1000, -1600, 0);
    M9TREE(-1000, -1600, 0);
    M9SIGN(300, -1200, 0);

        // Posicion inicial (uds DIV2): x,y = plano del suelo; z = altura
    x = 0;                // frente al porton (el porton esta en z=-1000)
    y = -1500;            // fuera de la muralla, mirando hacia el castillo
    z = 0;
    angle = -90000;       // mirando hacia +Z (hacia el porton)

    // Estado del modo 9
    m9.camera = id;
    m9.height = 160;      // altura del ojo (1.6 uds OBJ; muralla de 4 de alto)
    m9.angle  = 0;        // pitch (0 = horizonte)
    eye_base  = m9.height;
    radius    = 40;       // distancia minima a la pared (para MODE9_COLLISION)

    // START_MODE9(camara, m9, region) -- misma firma que start_mode8.
    //   camara = id del proceso camara.
    //   m9     = &m9.z (la DIRECCION del STRUCT; en modo 8 aqui iria el indice
    //            de m8, pero al ser m9 un struct del PRG hay que pasar su
    //            direccion. &m9.z = base del struct; usa &m9[n].z para un array
    //            de camaras). La DLL lee m9.camera/height/angle de ahi cada
    //            frame (no hace falta MODE9_SET ni MODE9_BIND aparte).
    //   region = 0 (pantalla completa; unica region soportada por la DLL).
    START_MODE9(id, &m9.z, 0);

    //MODE9_DOWNSCALE(2) reduce la resolucion del render del modo 9.
    // Es un truco empleado en juegos first person shooter de consolas de 16 bits
    // Cada valor de downscale reduce la resolucion a 1/2, 1/3 o 1/4 (y duplica lineas para llenar la pantalla).
    // 1 = resolucion completa (sin downscale, valor por defecto);
    // 2 = render a la mitad de resolucion (si usas 320x200 -> Se aplica 160x100);
    // 3 = un tercio (si usas 320x200 -> Se aplica 80x50);
    // 4 = un cuarto (si usas 320x200 -> Se aplica 40x25);
    // Acelera mucho el render a costa de una gran perdida de calidad (pixelado, texturas borrosas, mas aliasing). Ajustalo a tu gusto.
    //MODE9_DOWNSCALE(2);

    mouse_x_prev = mouse.x;
    fall_v   = 0;
    grounded = 1;
    bub_on   = 0;

    // HUD del juego (se crea aqui, ya pasada la pantalla de inicio).
    write(0, scr_x/2, scr_y-9, 4, "WASD+RATON  ESPACIO salto  ESC salir");
    write_int(0, 2, 2, 0, OFFSET fps);

    LOOP
        // --- ESC: dialogo 2D de confirmacion "Deseas salir? Si/No" ---------
        if (key(_esc))
            STOP_MODE9();                       // pausa el render 3D
            put_screen(0, 15);

            while (key(_esc)) FRAME; end        // soltar ESC
            delete_text(all_text);              // limpiar HUD/bocadillo
            delete_draw(all_drawing);
            bub_on = 0;
            sel = 0;                            // 0 = No (por defecto), 1 = Si
            draw(3,  0,  3, 0, 40, 68, scr_x-40, 132);             // fondo oscuro
            draw(2, 15, 15, 0, 40, 68, scr_x-40, 132);             // marco del panel
            write(0, scr_x/2, 84, 4, "Deseas salir?");
            write(0, scr_x/2-34, 112, 4, "Si");
            write(0, scr_x/2+34, 112, 4, "No");
            ed_cur = draw(2, 14, 15, 0, scr_x/2+18, 104, scr_x/2+50, 122);  // cursor en "No"
            repeat
                if (key(_left))  sel = 1; end
                if (key(_right)) sel = 0; end
                if (sel == 1) move_draw(ed_cur, 14, 15, scr_x/2-50, 104, scr_x/2-18, 122);
                else          move_draw(ed_cur, 14, 15, scr_x/2+18, 104, scr_x/2+50, 122); end
                FRAME;
            until (key(_enter))
            if (sel == 1) STOP_MODE9(); exit("Bye", 0); end
            // "No": limpiar dialogo, recrear HUD y reanudar el juego 3D.
            delete_text(all_text);
            delete_draw(all_drawing);
            clear_screen();
            write(0, scr_x/2, scr_y-9, 4, "WASD+RATON  ESPACIO salto  ESC salir");
            write_int(0, 2, 2, 0, OFFSET fps);
            START_MODE9(id, &m9.z, 0);
            while (key(_enter)) FRAME; end       // soltar intro
            mouse_x_prev = mouse.x;       // evitar tiron de camara al reanudar
        end

        // --- Movimiento lateral (strafe) ---
        if (key(_d)) xadvance(angle - 90000, ADV_UNIT); end
        if (key(_a)) xadvance(angle + 90000, ADV_UNIT); end

        // --- Avanzar / retroceder ---
        if (key(_w))
            if (key(_l_shift) or key(_r_shift)) advance(ADV_UNIT*2);
            else advance(ADV_UNIT); end
        end
        if (key(_s)) advance(-ADV_UNIT); end

        // --- Salto y gravedad (LA GRAVEDAD VIVE EN EL PRG) -------------------
        // El PRG integra la gravedad (z baja cada frame). El SUELO -incluidos
        // escalones y rampas- lo pone la DLL TRAS el FRAME: si el jugador se apoya
        // en algo, la DLL le SUBE z (ver MODE9_STEP). Por eso despues del FRAME
        // comprobamos si z subio respecto a lo que dejo la gravedad: si subio y
        // estabamos cayendo, es que tocamos suelo (grounded) y paramos la caida.
        if (key(_space) and grounded)
            fall_v   = -60;        // impulso de salto (negativo = hacia arriba)
            grounded = 0;
        end
        fall_v = fall_v + 6;                              // gravedad (subida)
        if (fall_v > 0) fall_v = fall_v + 8; end         // caida mas rapida (game feel)
        z = z - fall_v;            // caer (o subir, si fall_v<0 por el salto)
        if (z < 0) z = 0; end       // suelo de seguridad en y=0 (por si no hay malla)
        z_fell = z;                // z tras la gravedad, antes de que la DLL ajuste

        // --- Agacharse ---
        if (key(_control)) m9.height = eye_base * 2 / 3;
        else m9.height = eye_base; end

        // --- Raton: giro horizontal y pitch vertical (adaptado a la resolucion) -
        // Yaw: sensibilidad ~constante en cualquier modo (256000/scr_x = 800 @320).
        angle = angle - (mouse.x - mouse_x_prev) * 256000 / scr_x;
        // Pitch: mapeo ABSOLUTO de mouse.y. Centro = scr_y/2 (horizonte real);
        // factor 120000/scr_y para que toda la altura cubra +-60000 (=600 @200).
        m9.angle = (scr_y/2 - mouse.y) * 120000 / scr_y;
        if (m9.angle >  60000) m9.angle =  60000; end
        if (m9.angle < -60000) m9.angle = -60000; end

        if (mouse.x <= 0 or mouse.x >= scr_x-1) mouse.x = scr_x/2; end
        mouse_x_prev = mouse.x;

        // ===================== INTERCAMBIO DE MAPAS (INLINE) =================
        if (cooldown > 0)
            cooldown = cooldown - 1;
        else
            if (map_state == MAP_OUT)
                // Pegados a la puerta del torreon? -> ENTRAR
                dx = x - DOOR_X;  dy = y - DOOR_Y;  d2 = dx*dx + dy*dy;
                if (d2 < DOOR_R2)
                    fade_off();
                    frame;
                    fade_on();
                    frame;
                    signal(type M9TREE, s_kill);     // descargar props del patio
                    signal(type M9SIGN, s_kill);
                    // Recarga en caliente: el modo 9 sigue ACTIVO y la camara
                    // enlazada (id + &m9.z) persiste; LOAD_WLD9 solo cambia la malla.
                    LOAD_WLD9("OBJ/CASTIN.OBJ", "MAP/CASTIN_T.MAP", 100);  // 1 material (0)
                    SKY9_OFF();
                    SET_ENV_COLOR9(8, 8, 12);
                    NPC(0, 500, 50);                 // el NPC, ahora dentro (sobre el estrado)
                    THRONE(0, 575, 50);              // trono detras del NPC, en el estrado
                    LAMP(0, 300,  500);                 // lampara colgando del techo
                    LAMP(0, -150, 500);                 // lampara colgando del techo
                    TORCH( 470,  300, 250);          // 4 antorchas en las paredes
                    TORCH( 470, -100, 250);
                    TORCH(-470,  300, 250);
                    TORCH(-470, -100, 250);
                    x = 0;  y = -300;  z = 0;  angle = -90000;
                    m9.height = eye_base;  m9.angle = 0;
                    fall_v = 0;  grounded = 1;
                    map_state = MAP_IN;
                    cooldown  = SWAP_CD;
                end
            else
                // Pegados a la puerta de salida? -> SALIR
                dx = x - EXIT_X;  dy = y - EXIT_Y;  d2 = dx*dx + dy*dy;
                if (d2 < EXIT_R2)
                    fade_off();
                    frame;
                    fade_on();
                    frame;
                    signal(type NPC, s_kill);        // descargar NPC y props de la sala
                    signal(type THRONE, s_kill);
                    signal(type LAMP, s_kill);
                    signal(type TORCH, s_kill);
                    LOAD_WLD9("OBJ/CASTLE.OBJ", "MAP/CASTLE.MAP", 100);
                    MODE9_MATERIAL(1, "MAP/STAIRS.MAP");
                    MODE9_MATERIAL(2, "MAP/WATER.MAP");
                    LOAD_SKY9("MAP/SKY.MAP");
                    M9TREE(600, -1300, 0);
                    M9TREE(-600, -1300, 0);
                    M9TREE(1000, -1600, 0);
                    M9TREE(-1000, -1600, 0);
                    M9SIGN(300, -1200, 0);
                    x = 0;  y = -650;  z = 0;  angle = 90000;
                    m9.height = eye_base;  m9.angle = 0;
                    fall_v = 0;  grounded = 1;
                    map_state = MAP_OUT;
                    cooldown  = SWAP_CD;
                end
            end
        end

        // ===================== NOMBRE + BOCADILLO (estilo comic) ============
        // Se muestra SOLO si: estamos en la sala, CERCA del NPC y MIRANDOLO
        // (MODE9_PROJECT devuelve 1 = delante de la camara, y su x cae en pantalla).
        //   - El NOMBRE ("Nazuma") va ENCIMA del NPC (sobre su cabeza), sin tapar
        //     la cara, con una placa semitransparente para leerse.
        //   - El BOCADILLO con la frase va a un LADO del NPC; elige IZDA o DCHA
        //     segun donde haya mas sitio (si el NPC esta en la mitad izquierda de
        //     la pantalla, el bocadillo va a la derecha, y viceversa). El RABITO
        //     sale del lado del bocadillo que mira al NPC.
        // Ambos SIGUEN al NPC (se reposicionan con move_draw/move_text cada frame).
        show = 0;
        if (map_state == MAP_IN)
            bd2 = (x - NPC_X)*(x - NPC_X) + (y - NPC_Y)*(y - NPC_Y);
            if (bd2 < BUBBLE_R2)
                if (MODE9_PROJECT(NPC_X, NPC_Y, NPC_HEAD, OFFSET nsx, OFFSET nsy, OFFSET nsz))
                    if (nsx >= 0 and nsx < scr_x)   // el NPC esta en pantalla -> lo miramos
                        show = 1;
                    end
                end
            end
        end

        if (show)
            // Punto sobre la cabeza para el nombre (si no proyecta, usa la cabeza).
            if (MODE9_PROJECT(NPC_X, NPC_Y, NPC_TOP, OFFSET tsx, OFFSET tsy, OFFSET nsz) == 0)
                tsx = nsx;  tsy = nsy - 20;
            end
            if (tsy < 8) tsy = 8; end       // que el nombre no se salga por arriba

            // Lado del bocadillo segun el espacio disponible en pantalla.
            if (nsx < scr_x/2) cx = nsx + BUB_GAP + BUB_HW;   // NPC a la izda -> bocadillo a la DCHA
            else               cx = nsx - BUB_GAP - BUB_HW;   // NPC a la dcha -> bocadillo a la IZDA
            end
            if (cx < BUB_HW)         cx = BUB_HW;          end   // recortar para no salirse
            if (cx > scr_x - BUB_HW) cx = scr_x - BUB_HW;  end
            cy = nsy;                                            // a la altura de la cabeza
            if (cy < BUB_HH + 2)            cy = BUB_HH + 2;            end
            if (cy > scr_y - BUB_HH - 12)   cy = scr_y - BUB_HH - 12;   end
            // Borde del bocadillo que mira al NPC (de ahi sale el rabito).
            if (cx > nsx) tx = cx - BUB_HW;   else tx = cx + BUB_HW;   end

            if (bub_on == 0)
                // draw(tipo, color, opacidad 0..15, region, x0,y0, x1,y1)
                bub_fill   = draw(5,  0,  7, 0, cx-BUB_HW, cy-BUB_HH, cx+BUB_HW, cy+BUB_HH); // elipse rellena semitransp.
                bub_brd    = draw(4, 15, 15, 0, cx-BUB_HW, cy-BUB_HH, cx+BUB_HW, cy+BUB_HH); // borde de la elipse
                bub_tail   = draw(1, 15, 15, 0, tx, cy, nsx, nsy);                           // rabito hacia el NPC
                bub_spk    = write(0, cx, cy, 4, "Es peligroso andar solo");
                name_plate = draw(3,  0,  7, 0, tsx-24, tsy-7, tsx+24, tsy+7);               // placa del nombre
                name_txt   = write(0, tsx, tsy, 4, "Nazuma");                                // nombre encima del NPC
                bub_on     = 1;
            else
                // ya existe: solo reposicionar para seguir al NPC
                move_draw(bub_fill, 0,  7, cx-BUB_HW, cy-BUB_HH, cx+BUB_HW, cy+BUB_HH);
                move_draw(bub_brd, 15, 15, cx-BUB_HW, cy-BUB_HH, cx+BUB_HW, cy+BUB_HH);
                move_draw(bub_tail,15, 15, tx, cy, nsx, nsy);
                move_text(bub_spk, cx, cy);
                move_draw(name_plate, 0, 7, tsx-24, tsy-7, tsx+24, tsy+7);
                move_text(name_txt, tsx, tsy);
            end
        else
            if (bub_on == 1)
                delete_draw(bub_fill);
                delete_draw(bub_brd);
                delete_draw(bub_tail);
                delete_draw(name_plate);
                delete_text(bub_spk);
                delete_text(name_txt);
                bub_on = 0;
            end
        end

        FRAME;

        // Tras el FRAME la DLL ya pudo SUBIR z (escalon/rampa/apoyo en el suelo).
        // Si z quedo por encima de lo que dejo la gravedad y veniamos cayendo,
        // hemos tocado suelo: marcamos grounded y anulamos la velocidad de caida.
        if (z > z_fell - 1 and fall_v > 0)
            grounded = 1;
            fall_v   = 0;
        else
            grounded = 0;
        end
    END
END

//------------------------------------------------------------------------------
// SPRITES billboard (estilo modo 7): ctype = c_m9 + coords de MUNDO + graph/size.
// Se destruyen con signal(type ..., s_kill) al cambiar de mapa.
//------------------------------------------------------------------------------
PROCESS NPC(wx, wy, wz)
PRIVATE
    npc_gr[] = 8, 8,1,2,3,4,5,6,7;
BEGIN
    ctype = c_m9;
    xgraph = &npc_gr[0];
    size  = 100;
    angle = pi/2;
    x = wx; y = wy; z = wz;
    LOOP
        FRAME;
    END
END

PROCESS M9TREE(wx, wy, wz)
BEGIN
    ctype = c_m9;
    graph = 9;
    size  = 100;
    angle = 0;
    x = wx; y = wy; z = wz;
    LOOP
        FRAME;
    END
END

PROCESS M9SIGN(wx, wy, wz)
BEGIN
    ctype = c_m9;
    graph = 10;
    size  = 100;
    angle = 0;
    x = wx; y = wy; z = wz;
    LOOP
        FRAME;
    END
END

//------------------------------------------------------------------------------
// Billboards del INTERIOR (graficos id >= 11 en FPG/MODE9.FPG):
//   11 = antorcha,  12 = lampara de techo,  13 = trono.
// Se crean al entrar a la sala y se destruyen con signal(type ...) al salir.
//------------------------------------------------------------------------------
PROCESS TORCH(wx, wy, wz)
BEGIN
    ctype = c_m9;
    graph = 11;
    size  = 200;
    angle = 0;
    x = wx; y = wy; z = wz;
    LOOP
        FRAME;
    END
END

PROCESS LAMP(wx, wy, wz)
BEGIN
    ctype = c_m9;
    graph = 12;
    size  = 300;
    angle = 0;
    x = wx; y = wy; z = wz;
    LOOP
        FRAME;
    END
END

PROCESS THRONE(wx, wy, wz)
BEGIN
    ctype = c_m9;
    graph = 13;
    size  = 400;
    angle = 0;
    x = wx; y = wy; z = wz;
    LOOP
        FRAME;
    END
END
