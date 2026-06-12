//------------------------------------------------------------------------------
// TITLE:       MODE9.PRG
// AUTHOR:      Sebastian J. MONCHO MAQUET
// DATE:        08/06/2026
// DESCRIPTION: Ejemplo del "modo 9": desplazamiento por un escenario 3D (.OBJ)
//              con camara en primera persona (estilo Doom / Elder Scrolls Arena)
//              usando MODE9.DLL. Sobre el escenario se ven objetos 2D como en
//              el modo 7 (sprites con escala segun distancia, aka "billboards").
//              Este RPG es identico al MODE9_03.PRG pero con animaciones para el gato 3D.
//
// CONTROLES:
//   W / S         : avanzar / retroceder (SHIFT = correr)
//   A / D         : desplazamiento lateral (strafe)
//   Raton         : girar (horizontal) y mirar (vertical / pitch)
//   ESPACIO       : saltar
//   CTRL          : agacharse
//   ESC           : salir
//
// FICHEROS NECESARIOS:
//   CASTLE.OBJ    : Escenario 3D (castillo + escalera en el patio + lago isla;
//                   3 materiales por 'usemtl': Castle, Stairs, Water).
//   MAP/CASTLE.MAP: Textura del castillo (material 0, por defecto).
//   MAP/STAIRS.MAP: Textura de los peldaños de la escalera (material 1).
//   MAP/WATER.MAP : Textura del lago que rodea la isla (material 2).
//   FPG/MODE9.FPG : Gráficos de los billboards.
//   MAP/SKY.MAP    : Textura del cielo panorámico (skybox).
//
// SOBRE m9 Y c_m9:
//   En DIV/DIV2, una DLL no puede crear STRUCTs ni c_types del lenguaje DIV2,
//   asi que aqui se declaran: el STRUCT m9 (mismos campos que m8: z, camera, height, angle)
//   y la constante c_m9 = 200.
//   La camara se activa con START_MODE9(id,&m9.z,0).
//   Los sprites se ponen con ctype = c_m9 y coordenadas de MUNDO (x,y,z) + size
//   + graph; el modo 9 los dibuja como billboards escalados por distancia SIN
//   llamar a nada cada frame (igual que c_m7 en el modo 7). c_m9 DEBE valer lo
//   mismo en el PRG y en la DLL (200 por defecto; cambiable con MODE9_CTYPE).
//------------------------------------------------------------------------------
PROGRAM MODE9_DEMO;

IMPORT "MODE9.DLL";

CONST
    SCREEN_X   = 320;     // Ancho de la resolucion de la pantalla
    SCREEN_Y   = 200;     // Alto de la resolucion de la pantalla
    GAME_FPS   = 60;      // FPS objetivo
    ADV_UNIT   = 30;      // velocidad de paso (uds DIV2)
    c_m9       = 200;     // c_type "logico" del modo 9 (convencion, ver cabecera)
    CAT_FRAMES = 10;      // fotogramas del idle del gato (CAT0.OBJ..CAT9.OBJ)
    CAT_LOOP   = 300;     // duracion del bucle en centisegundos (300 = 3 s, ver timer)

GLOBAL
    STRUCT m9            // STRUCT de camara del modo 9 (mismos campos que m8). &m9.z = base del struct.
        z;
        camera;
        height;
        angle;
    END
    scr_x;                // resolucion real (la lee la DLL): raton/camara a
    scr_y;                // cualquier resolucion, no solo 320x200
    cat_h;                // handle del modelo 3D del gato (LOAD_OBJ9)
BEGIN
    // Es probable que los assets tarden en cargar en hardware real.
    // Si te salta la expcion 142, puedes decomentar esta linea para ignorarla.
    //ignore_error(142);

    set_mode(SCREEN_X * 1000 + SCREEN_Y);
    set_fps(GAME_FPS, 0);

    scr_x = MODE9_SCRW();   // MODE9_SCRW() y MODE9_SCRH() se pueden llamar en cualquier momento, no solo al inicio, por si quieres cambiar la resolucion con set_mode durante el juego.
    scr_y = MODE9_SCRH();

    load_fpg("FPG/MODE9.FPG"); // cargar el FPG del juego

    MODE9_LOADFPG("FPG/MODE9.FPG"); // FPG del modo 9 para los billboards

    // --- Cargar el escenario 3D (UNA sola llamada: malla + textura + escala) --
    // LOAD_WLD9(obj, textura, unitscale).  unitscale = 100 uds DIV2 por unidad
    // OBJ. Si la textura no existe (o pones ""), el render cae a flat-shading.
    // La textura de LOAD_WLD9 es el material 0 y se aplica por defecto a todos los materiales.
    // Si cambias 100 por 200, el escenario será el doble de grande.
    // La carga del escenario es bloqueante (hasta que no acabe la acción, no se continúa).
    // Si cargas escenarios enormes, puede saltar al excepcion 142 ("El proceso parece bloqueado, esperando FRAME")
    LOAD_WLD9("OBJ/CASTLE.OBJ", "MAP/CASTLE.MAP", 100);

    // --- MULTIMATERIALES -----------------------------------------------------
    // CASTLE.OBJ tiene 3 materiales por 'usemtl' (en orden): 0=Castle, 1=Stairs,
    // 2=Water. MODE9_MATERIAL(idx, map) asigna una textura a cada uno. El 0 ya lo
    // puso LOAD_WLD9 (CASTLE.MAP); aqui ponemos la escalera y el mar. Si un .MAP
    // no existe aun, ese material cae al 0 (CASTLE.MAP) sin fallar.
    MODE9_MATERIAL(1, "MAP/STAIRS.MAP");   // peldanos de la escalera del patio
    MODE9_MATERIAL(2, "MAP/WATER.MAP");    // agua que rodea la isla

    // MODE9_CULL(1) activa el backface culling (no dibujar las caras que se ven por detras; mas rapido).
    // Por defecto está activado. Si deseseas desactivarlo haz MODE9_CULL(0);
    //MODE9_CULL(0);

    // MODE9_FOV cambia el campo de vision horizontal (FOV) en grados.
    // El valor por defecto es ~71, que es el FOV "canonico" de los juegos 3D de la época
    // Un valor menor (60) hace zoom, y un valor mayor (90 o 110) da un efecto de gran angular.
    // Cambiar el FOV puede afectar la sensacion de velocidad y la distorsion de las texturas, asi que ajustalo a tu gusto.
    //MODE9_FOV(90);

    // MODE9_PERSPECTIVE(1) Corrige gran parte de los defectos gráficos de las texturas, pero impacta mucho en el rendimiento.
    // Por defecto esta desactivado (0), lo que da un efecto de distorsion de las texturas.
    //MODE9_PERSPECTIVE(1);

    // Far clip ("flopping"): no dibujar geometria mas alla de N uds DIV2.
    // Por defecto está a 0 (sin limite).
    // Si activas niebla con SET_FOG9, se auto-activa el FARCLIP con el valor de 'far' de la niebla (lo que queda tras la niebla es del color de fondo, no se dibuja).
    //MODE9_FARCLIP(1000);

    // MODE9_COLLISION activa y desactiva las colisiones: con 1, advance/xadvance NO atraviesan las paredes (la distancia
    // minima a la pared es el campo 'radius' del proceso camara). Con 0 puedes atravesar paredes.
    //MODE9_COLLISION(0);

    // MODE9_STEP(INT X) se usa para subir escaleras y rampas.
    // El valor es la altura max (uds DIV2) que la camara sube sola. Los muros
    // mas bajos NO bloquean (se suben como un peldaño) y la camara se apoya
    // en el suelo que tiene debajo (rampas inclinadas, escalones). 0 = desactivado.
    // La DLL no aplica GRAVEDAD, la gravedad la tienes que gestionar en el PRG (ver el bloque de salto/gravedad de 'camara');
    // Ojo, para que funcione las colisiones deben de estar activadas (no va con MODE9_COLLISION(0)).
    MODE9_STEP(70);

    // --- Ambiente y niebla (color en rango 0..100, convencion DIV2) ----------
    // SET_ENV_COLOR9 fija el color de FONDO y tambien el color de la NIEBLA.
    // 0..100 por canal: 100=blanco, 0=negro. SET_FOG9(near,far) funde hacia ese
    // color crecientemente con la DISTANCIA: transparente hasta 'near' y opaco en
    // 'far' (gradiente suave, como el modo 8). OJO: a diferencia del modo 8
    // (0..100), aqui near/far son DISTANCIAS en uds DIV2; lo normal es far ~ 2*near.
    //SET_ENV_COLOR9(60, 72, 88);        // cielo amarillento, impacta en el color de la niebla
    //SET_FOG9(300, 1400);               // niebla: near=300  far=1400 (uds DIV2)

    // Por defecto la niebla no está activada, pero si la activas luego la puedes
    // desactivar con MODE9_FOG_OFF.
    //MODE9_FOG_OFF();

    // --- Cielo panoramico (skybox) -------------------------------------------
    // LOAD_SKY9(map): textura .MAP que envuelve 360 en horizontal; sustituye el
    // color de fondo plano (SET_ENV_COLOR9).
    // Gira con la camara (yaw) y se desplaza con el pitch.
    LOAD_SKY9("MAP/SKY.MAP");

    // --- NPC 3D animado (objeto OBJ texturado en el mundo) -------------------
    // LOAD_OBJ9_ANIM carga una animacion por vertex morphing: los fotogramas son
    // OBJ/CAT0.OBJ..OBJ/CAT9.OBJ (misma topologia, solo cambian las posiciones) con una
    // unica textura CAT.MAP compartida. Ocupa UN SOLO slot de los 8 de la DLL,
    // independientemente del numero de fotogramas. La carga se separa de NPC3D y
    // se llama con FRAME entre medias para refrescar antes de que arranquen los
    // procesos (y porque parsear 10 OBJ tarda varios segundos en disco real).
    cat_h = LOAD_OBJ9_ANIM("OBJ/CAT", "MAP/CAT.MAP", 50, CAT_FRAMES);
    /*
    // Los modelos se cargan en segundo plano, mientras se ejecuta el juego
    // No obstante, con MODE9_OBJ9_READY puedes consultar si el modelo ha
    // acabado de cargarse en memoria (1 = listo, 0 = aun cargando).
    // Puede servir, por ejemplo, para hacer pantallas de carga
    // Esto es opcional, puedes llamar a DRAW_OBJ9 o DRAW_OBJ9_ANIM con un handle aun no cargado (0) y simplemente no dibuja nada en ese FRAME.
    // Pero ten en cuenta que, mientras el modelo se carga, el juego se ralentiza
    write(0, SCREEN_X/2, SCREEN_Y/2, 4, "Cargando modelo 3D...");
    while(MODE9_OBJ9_READY(cat_h) == 0)
        FRAME;
    END;
    delete_text(ALL_TEXT);*/

    // --- Procesos ------------------------------------------------------------
    camara();
    NPC3D(cat_h, 400, -1300, 0);
    NPC(0, 600, 0);          // un sprite angular "modo 7" en el patio del castillo, representa un NPC
    M9TREE(600, -1300, 0);   // un sprite plano, representa un arbol
    M9TREE(-600, -1300, 0);  // un sprite plano, representa un arbol
    M9TREE(1000, -1600, 0);  // un sprite plano, representa un arbol
    M9TREE(-1000, -1600, 0); // un sprite plano, representa un arbol
    M9SIGN(300, -1200, 0);   // un cartel plano con textura

    write(0, 2, scr_y-9, 0, "WASD+RATON mover  ESPACIO salto  ESC salir");
    write_int(0, 0, 0, 0, OFFSET fps);
END


//------------------------------------------------------------------------------
// Proceso CAMARA: se mueve por el escenario como en el modo 8.
//------------------------------------------------------------------------------
PROCESS camara()
PRIVATE
    mouse_x_prev;
    eye_base;
    fall_v;        // velocidad de caida (la gravedad la integra el PRG)
    grounded;      // 1 si la DLL nos sostiene en un suelo/escalon/rampa
    z_fell;        // z tras aplicar la gravedad, ANTES de que la DLL ajuste
BEGIN
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

    //MODE9_DOWNSCALE(2) reduce la resolución del render del modo 9.
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


    load_pal("div2.pal");
    LOOP
        if (key(_esc)) STOP_MODE9(); exit("Bye", 0); end

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
// Procesos de SPRITE billboard (estilo modo 7): basta con ponerle ctype = c_m9 y
// sus coordenadas de MUNDO (x,y = suelo, z = altura), graph y size. El modo 9
// lo dibuja automaticamente como sprite 2D escalado segun la distancia, SIN
// llamar a nada cada frame (igual que un proceso c_m7 en el modo 7).
//   - x, y : posicion en el plano del suelo (uds DIV2)
//   - z    : altura (uds DIV2)
//   - size : escala base en % (la distancia la aplica el modo 9 encima)
//
// OCLUSION: el modo 9 oculta el sprite si queda tapado por la geometria (test
// de z-buffer). Se puede desactivar con MODE9_OCCLUDE(0).
//------------------------------------------------------------------------------
PROCESS NPC(wx, wy, wz)
PRIVATE
    npc_gr[] = 8, 8,1,2,3,4,5,6,7;   // N=8 graficos en 360º

BEGIN
    ctype = c_m9;         // <-- el modo 9 lo pinta como billboard automaticamente
    //graph = 8;          // grafico de MODE9.FPG, billboard plano (1 cara)
    xgraph = &npc_gr[0];  // grafico de MODE9.FPG, billboard con multiples caras, en funcion del angulo de vision
    size  = 100;          // escala base (%); el modo 9 la modula por distancia
    angle = 0;
    x = wx; y = wy; z = wz;   // posicion en el MUNDO (el proceso la conserva)
    LOOP
        FRAME;
    END
END

PROCESS M9TREE(wx, wy, wz)
BEGIN
    ctype = c_m9;         // <-- el modo 9 lo pinta como billboard automaticamente
    graph = 9;          // grafico de MODE9.FPG, billboard plano (1 cara)
    size  = 100;          // escala base (%); el modo 9 la modula por distancia
    angle = 0;
    x = wx; y = wy; z = wz;   // posicion en el MUNDO (el proceso la conserva)
    LOOP
        FRAME;
    END
END

PROCESS M9SIGN(wx, wy, wz)
BEGIN
    ctype = c_m9;         // <-- el modo 9 lo pinta como billboard automaticamente
    graph = 10;          // grafico de MODE9.FPG, billboard plano (1 cara)
    size  = 100;          // escala base (%); el modo 9 la modula por distancia
    angle = 0;
    x = wx; y = wy; z = wz;   // posicion en el MUNDO (el proceso la conserva)
    LOOP
        FRAME;
    END
END

//------------------------------------------------------------------------------
// NPC3D: objeto 3D texturado en el mundo. Usa DRAW_OBJ9 cada frame para que la
// DLL lo renderice como malla 3D (con z-buffer respecto al escenario).
//   handle : devuelto por LOAD_OBJ9 (0 = modelo no cargado -> no dibuja nada)
//   wx, wy : posicion inicial en el plano del suelo (uds DIV2)
//   wz     : altura inicial (uds DIV2; 0 = al nivel del suelo)
//
// El proceso mantiene sus coordenadas (x,y,z,angle) como cualquier proceso DIV2:
// puedes moverlo con advance(), cambiar angle, etc. en subclases.
// DRAW_OBJ9_ANIM(handle, x, y, z, angle, fpos) se llama antes de cada FRAME.
// 'fpos' es la posicion de la animacion en 1/256 de fotograma; aqui se deriva de
// timer[0] (centisegundos) para un bucle continuo de CAT_LOOP cs = 3 s. Para un
// modelo cargado con LOAD_OBJ9 (estatico) DRAW_OBJ9_ANIM tambien vale: la DLL ve
// nframes=1 y dibuja siempre el frame 0, ignorando fpos.
//------------------------------------------------------------------------------
PROCESS NPC3D(handle, wx, wy, wz)
PRIVATE
    fpos;
BEGIN
    x = wx; y = wy; z = wz;
    angle = pi;
    LOOP
        if (handle > 0)
            // bucle de CAT_LOOP cs -> 0..CAT_FRAMES*256; la DLL interpola entre
            // keyframes (idle suave) y hace bucle al volver al frame 0.
            fpos = (timer[0] % CAT_LOOP) * (CAT_FRAMES * 256) / CAT_LOOP;
            DRAW_OBJ9_ANIM(handle, x, y, z, angle, fpos);
        end
        FRAME;
    END
END
