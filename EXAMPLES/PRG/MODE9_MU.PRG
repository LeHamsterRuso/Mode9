//------------------------------------------------------------------------------
// TITLE:       MODE9_2P.PRG
// AUTHOR:      Sebastian J. MONCHO MAQUET
// DATE:        07/06/2026
// DESCRIPTION: Ejemplo del "modo 9" a DOS JUGADORES con PANTALLA PARTIDA
//              (split-screen) usando MODE9.DLL. Dos camaras independientes, cada
//              una con su STRUCT m9 y su viewport (mitad superior / inferior).
//
//   JUGADOR 1 (mitad superior):  W/S avanzar-retroceder   A/D girar   ESPACIO saltar
//   JUGADOR 2 (mitad inferior):  CURSORES arr/abj avanzar   izq/der girar   CTRL saltar
//   ESC: salir
//
//   GRAVEDAD + ESCALERAS/RAMPAS: cada camara cae por gravedad (la integra el PRG)
//   y la DLL la apoya en el suelo, sube peldaños/rampas (MODE9_STEP). En el patio
//   hay una pequena escalera (material Stairs) y el mapa lo rodea un lago (Water).
//
//   Ninguno usa xadvance (no hay strafe): izquierda/derecha GIRAN la camara,
//   como en un FPS clasico.
//
//   COLISIONES: MODE9_COLLISION(1) hace que advance NO atraviese las paredes;
//   la distancia minima a la pared es el campo 'radius' de cada camara.
//
// FICHEROS: CASTLE.OBJ + MAP/CASTLE.MAP (como en MODE9.PRG).
//------------------------------------------------------------------------------
PROGRAM MODE9_2P;

IMPORT "MODE9.DLL";

CONST
    SCREEN_X   = 320;     // Ancho de la resolucion de la pantalla
    SCREEN_Y   = 200;     // Alto de la resolucion de la pantalla
    GAME_FPS   = 60;      // FPS objetivo
    ADV_UNIT   = 30;      // velocidad de paso (uds DIV2)
    c_m9       = 200;     // c_type "logico" del modo 9 (convencion, ver cabecera)
    TURN       = 7000;    // velocidad de giro (milesimas de grado por frame)

GLOBAL
    // Array de dos camaras m9 (mismos campos que m8). &m9[n].z = base del n.
    STRUCT m9[2]
        z;
        camera;
        height;
        angle;
    END

PRIVATE
    c1; c2;               // ids de los procesos camara
    scrw; scrh;           // resolucion real (viewports a cualquier resolucion)
BEGIN
    // Es probable que los assets tarden en cargar en hardware real.
    // Si te salta la expcion 142, puedes decomentar esta linea para ignorarla.
    //ignore_error(142); 

    set_mode(SCREEN_X * 1000 + SCREEN_Y);
    set_fps(GAME_FPS, 0);
    load_fpg("FPG/MODE9.FPG"); // cargar el FPG del juego

    MODE9_LOADFPG("FPG/MODE9.FPG"); // FPG del modo 9 para los billboards

    // --- Cargar el escenario 3D (UNA sola llamada: malla + textura + escala) --
    // LOAD_WLD9(obj, textura, unitscale).  unitscale = 100 uds DIV2 por unidad
    // OBJ. Si la textura no existe (o pones ""), el render cae a flat-shading.
    // La textura de LOAD_WLD9 es el material 0 y se aplica por defecto a todos los materiales.
    // Si cambias 100 por 200, el escenario será el doble de grande.
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

    // --- Resolucion real (para los viewports a cualquier resolucion) ----------
    scrw = MODE9_SCRW();
    scrh = MODE9_SCRH();

    // --- Crear las DOS camaras ANTES de START_MODE9 --------------------------
    // Cada player_cam fija m9[pl].camera=id en su BEGIN, asi que tras crearlos
    // los STRUCT ya apuntan a su proceso. Sin esto, c1/c2 valen 0 y START_MODE9
    // no encuentra camara -> pantalla negra.
    c1 = player_cam(0,   0, -1500, -90000);   // jugador 1: fuera, mirando al porton
    c2 = player_cam(1, 100, -1500, -90000);   // jugador 2: fuera, mirando al porton

    // Split-screen POR REGIONES (como las ventanas de modo 8). Se define el rect
    // de cada region con MODE9_REGION(n,x,y,w,h) y luego se arranca una camara en
    // ella con START_MODE9(cam, &m9, n) -- igual que start_mode8(cam, m8, region).
    // (La DLL no ve las regiones de define_region() de DIV2, por eso las registra
    // ella misma; MODE9_REGION es el equivalente de define_region para el modo 9.)
    define_region(1, 0,        0, scrw, scrh/2);   // region DIV2: mitad superior
    define_region(2, 0, scrh/2,    scrw, scrh/2);   // region DIV2: mitad inferior
    MODE9_REGION(1, 0,        0, scrw, scrh/2);   // misma region para el modo 9
    MODE9_REGION(2, 0, scrh/2,    scrw, scrh/2);
    START_MODE9(c1, &m9[0].z, 1);                  // jugador 1 -> region 1 (arriba)
    START_MODE9(c2, &m9[1].z, 2);                  // jugador 2 -> region 2 (abajo)
    //MODE9_FOV(90);         // FOV mas amplio (def ~71). Afecta a ambas vistas.
    //MODE9_DOWNSCALE(2);   // Si va lento: render a menor resolucion (2/3/4) duplicando lineas
    //MODE9_PERSPECTIVE(1);  // Texturas perfectas (1/w por pixel) en vez de afin a 16px (mas lento)

    NPC(0, 600, 0);          // un sprite angular "modo 7" en el patio del castillo, representa un NPC
    M9TREE(600, -1300, 0);   // un sprite plano, representa un arbol
    M9TREE(-600, -1300, 0);  // un sprite plano, representa un arbol
    M9TREE(1000, -1600, 0);  // un sprite plano, representa un arbol
    M9TREE(-1000, -1600, 0); // un sprite plano, representa un arbol
    M9SIGN(300, -1200, 0);   // un cartel plano con textura

    write(0, 2, SCREEN_Y -9, 0, "P1 AWSD  P2 IZQ/DER/ARR/ABA  ESC salir");
    write_int(0, 0, 0, 0, OFFSET fps);

    LOOP
        if (key(_esc)) STOP_MODE9(); exit("Bye", 0); end
        FRAME;
    END
END


//------------------------------------------------------------------------------
// PROCESO camara de un jugador.
//   pl    : indice del jugador (0 = teclas WASD, 1 = cursores)
//   sx,sy : posicion inicial en el suelo (uds DIV2: x=mundo X, y=mundo Z)
//   sang  : angulo inicial
//------------------------------------------------------------------------------
PROCESS player_cam(pl, sx, sy, sang)
PRIVATE
    npc_gr[] = 8, 8,1,2,3,4,5,6,7;   // N=8 graficos en 360º
    fall_v;        // velocidad de caida (la gravedad la integra el PRG)
    grounded;      // 1 si la DLL nos sostiene en un suelo/escalon/rampa
    z_fell;        // z tras la gravedad, ANTES de que la DLL ajuste
    jumpk;         // tecla de salto de este jugador
BEGIN
    x = sx; y = sy; z = 0;
    angle  = sang;
    radius = 40;              // distancia minima a la pared (uds DIV2)
    ctype  = c_m9;            // marcador visible para el OTRO jugador (billboard)
    //graph = 8;            // grafico de MODE9.FPG
    xgraph = &npc_gr[0];
    size   = 100;             // escala base (%); el modo 9 la modula por distancia

    // Estado del modo 9 de ESTE jugador (su propio m9[pl])
    m9[pl].camera = id;
    m9[pl].height = 160;     // altura del ojo
    m9[pl].angle  = 0;       // pitch (0 = horizonte)

    fall_v   = 0;
    grounded = 1;
    if (pl == 0) jumpk = _space; else jumpk = _control; end   // salto por jugador

    LOOP
        if (pl == 0)
            // --- Jugador 1: WASD (+ESPACIO salto) ---
            if (key(_w)) advance(ADV_UNIT);  end
            if (key(_s)) advance(-ADV_UNIT); end
            if (key(_a)) angle += TURN;      end    // girar a la izquierda
            if (key(_d)) angle -= TURN;      end    // girar a la derecha
        else
            // --- Jugador 2: cursores (+CTRL salto) ---
            if (key(_up))    advance(ADV_UNIT);  end
            if (key(_down))  advance(-ADV_UNIT); end
            if (key(_left))  angle += TURN;      end
            if (key(_right)) angle -= TURN;      end
        end

        // --- Gravedad (en el PRG) + suelo/escalon/rampa (lo pone la DLL) -------
        // Mismo patron que MODE9.PRG: integramos la gravedad (z baja); tras el
        // FRAME la DLL puede SUBIR z si nos apoyamos en algo (ver MODE9_STEP).
        if (key(jumpk) and grounded)
            fall_v   = -60;        // impulso de salto (negativo = hacia arriba)
            grounded = 0;
        end
        fall_v = fall_v + 6;                              // gravedad (subida)
        if (fall_v > 0) fall_v = fall_v + 8; end         // caida mas rapida (game feel)
        z = z - fall_v;            // caer (o subir, si fall_v<0 por el salto)
        if (z < 0) z = 0; end       // suelo de seguridad en y=0
        z_fell = z;                // z tras la gravedad, antes de que la DLL ajuste

        FRAME;

        // Si la DLL nos dejo por encima de lo que dejo la gravedad y caiamos,
        // tocamos suelo: grounded y paramos la caida.
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
