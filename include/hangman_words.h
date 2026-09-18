#pragma once
#ifndef HANGMAN_WORDS_H
#define HANGMAN_WORDS_H

#include <Arduino.h>

// =============================================================================
// DICCIONARIO EN ESPAÑOL PARA HANGMAN (ESP32-C6)
// Palabras de 4 a 8 letras, sin tildes ni 'Ñ', almacenadas en memoria Flash (PROGMEM)
// =============================================================================
const char words_es[][9] PROGMEM = {
    // --- Animales ---
    "PERRO", "GATO", "TIGRE", "LEON", "LOBO", "ZORRO", "CABALLO", "BURRO", "OVEJA",
    "VACA", "TORO", "RATA", "RATON", "MONO", "PUMA", "CIERVO", "AGUILA", "HALCON", "PATIN",
    "CISNE", "LORO", "BUHO", "PATO", "POLLO", "GALLO", "CERDO", "CONEJO", "RANA", "SAPO",
    "DELFIN", "TIBURON", "PULPO", "BALLENA", "CEBRA", "JIRAFA", "CAMELLO", "CANGURO", "KOALA",
    "FOCA", "MORSA", "NUTRIA", "CASTOR", "LINCE", "JAGUAR", "HIENA", "CHACAL", "GORILA",
    "LEMUR", "PANTERA", "TUCAN", "COLIBRI", "FLAMENCO", "PELICANO", "GAVIOTA", "CUERVO",

    // --- Naturaleza y Geografía ---
    "PLAYA", "BOSQUE", "MONTE", "CAMPO", "FUEGO", "TIERRA", "VIENTO", "LLUVIA", "PIEDRA",
    "LAGO", "ISLA", "CIELO", "NUBE", "VOLCAN", "SELVA", "VALLE", "CUEVA", "COSTA",
    "ROCA", "ARENA", "NIEVE", "RAYO", "TRUENO", "PLANTA", "ARBOL", "FLOR", "HOJA", "RAMA",
    "HIERBA", "FRUTO", "SEMILLA", "DESIERTO", "OCEANO", "PRADERA", "SENDERO",
    "CASCADA", "COLINA", "PRADO", "FUENTE", "AURORA", "COMETA", "ESTRELLA", "PLANETA",

    // --- Objetos del Hogar y Vida Cotidiana ---
    "MESA", "SILLA", "PUERTA", "VENTANA", "LIBRO", "PAPEL", "LAPIZ", "RELOJ", "BOLSO",
    "LLAVE", "CAMA", "SOFA", "VASO", "PLATO", "TAZA", "CUCHARA", "TENEDOR", "CUCHILLO",
    "OLLA", "SARTEN", "ESPEJO", "CUADRO", "LAMPARA", "COJIN", "MANTA", "TOALLA", "JABON",
    "CEPILLO", "BOTON", "AGUJA", "HILO", "TIJERA", "CAJA", "MALETA", "MOCHILA", "CANDADO",
    "CUERDA", "BOTELLA", "CUADERNO", "CARTA", "SOBRE", "CARTERA", "DADO", "NAIPE", "JUGUETE",
    "PELOTA", "GLOBO", "ESCALERA", "MARTILLO", "CLAVO", "TORNILLO", "TALADRO", "PINCEL",
    "BROCHA", "PINTURA", "TIESTO", "CUBETA", "BALDE", "ESCOBA", "TRAPO", "REGALO",

    // --- Comida y Bebida ---
    "QUESO", "FRUTA", "CARNE", "LECHE", "ARROZ", "HUEVO", "SOPA", "PASTA", "PESCADO",
    "AGUA", "JUGO", "VINO", "CAFE", "DULCE", "PASTEL", "TORTA", "GALLETA", "MANZANA",
    "PERA", "PLATANO", "NARANJA", "LIMON", "FRESA", "CEREZA", "MELON", "SANDIA", "TOMATE",
    "PATATA", "CEBOLLA", "MAIZ", "TRIGO", "ACEITE", "AZUCAR", "MANTECA", "HELADO", "CREMA",
    "HARINA", "VINAGRE", "CANELA", "VAINILLA", "MANGO", "CIRUELA", "DURAZNO", "ALMENDRA",
    "NUEZ", "AVELLANA", "DATIL", "COCO", "CACAO", "SALSA", "CALDO",

    // --- Vehículos y Transporte ---
    "COCHE", "BARCO", "TREN", "AVION", "CAMION", "MOTO", "BARCA", "METRO", "TAXI",
    "COHETE", "TRACTOR", "CANOA", "VELERO", "TRANVIA", "FURGON", "PATINETE", "CRUCERO",
    "AVIONETA", "CARRO", "CARRUAJE",

    // --- Lugares y Construcciones ---
    "CIUDAD", "PUEBLO", "CAMINO", "PUENTE", "TORRE", "CASTILLO", "HOTEL", "BANCO",
    "PARQUE", "JARDIN", "TEATRO", "CINE", "CALLE", "PLAZA", "PUERTO", "MERCADO",
    "TIENDA", "MUSEO", "TEMPLO", "FARO", "GRANJA", "MOLINO", "PALACIO", "PISTA",
    "HOSPITAL", "ESCUELA", "COLEGIO", "CABANA", "REFUGIO", "ESTACION", "FABRICA",
    "ESTADIO", "GIMNASIO", "CIRCO", "BODEGA", "ALMACEN",

    // --- Profesiones y Roles ---
    "AMIGO", "FAMILIA", "MEDICO", "DOCTOR", "PINTOR", "PILOTO", "CHEF", "JUEZ",
    "ACTOR", "POETA", "GUIA", "MUSICO", "HEROE", "REINA", "CAPITAN", "SOLDADO",
    "POLICIA", "BOMBERO", "MARINERO", "MINERO", "CARTERO", "PASTOR", "GRANJERO",
    "ESPIA", "ALUMNO", "MAESTRO", "ESCRITOR", "CANTANTE", "BAILARIN", "ATLETA",
    "SASTRE", "JUGADOR", "MAGO",

    // --- Música, Arte y Cultura ---
    "FIESTA", "MUSICA", "PINTURA", "CANCION", "DANZA", "BAILE", "POEMA", "RIMA",
    "NOTA", "PIANO", "GUITARRA", "VIOLIN", "FLAUTA", "TAMBOR", "TROMPETA", "ARPA",
    "RITMO", "SONIDO", "CORO", "DISCO", "RADIO", "MAGIA", "TECLADO",

    // --- Ropa y Accesorios ---
    "CAMISA", "PANTALON", "FALDA", "VESTIDO", "ZAPATO", "BOTA", "GUANTE", "GORRO",
    "SOMBRERO", "CINTURON", "ANILLO", "COLLAR", "CORONA", "CORBATA", "CAPA", "ABRIGO",
    "CHAQUETA", "BUFANDA", "CHALECO", "PULLOVER", "GAFAS", "LENTES",

    // --- Conceptos, Colores y Emociones ---
    "ROJO", "AZUL", "VERDE", "NEGRO", "BLANCO", "AMARILLO", "ROSA", "GRIS", "DORADO",
    "PLATA", "CALOR", "FRIO", "TIEMPO", "FUERZA", "AMOR", "SUERTE", "VALOR",
    "VIAJE", "DESTINO", "JUEGO", "VICTORIA", "CAMPEON", "PREMIO", "TESORO", "MISTERIO",
    "SECRETO", "AVENTURA", "ENIGMA", "CORAZON", "MENTE", "ESPIRITU"
};

const uint16_t WORD_COUNT = sizeof(words_es) / sizeof(words_es[0]);

#endif
