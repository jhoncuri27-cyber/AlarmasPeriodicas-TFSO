
#include "kernel/types.h"
#include "user/user.h"

static volatile int alarmas;
static volatile int en_funcion;
static volatile int anidada;
static int funcion_lenta;

static void
carga_cpu(void)
{
  for (volatile int i = 0; i < 100000; i++) {
  }
}

static void
ocupar_cpu(int ticks)
{
  int inicio = uptime();

  while (uptime() - inicio < ticks)
    carga_cpu();
}

static int
esperar_alarmas(int cantidad)
{
  int inicio = uptime();

  while (alarmas < cantidad && uptime() - inicio < 120)
    carga_cpu();

  return alarmas >= cantidad;
}

static void
atender_alarma(void)
{
  if (en_funcion) {
    anidada = 1;
  } else {
    en_funcion = 1;
    alarmas++;
    if (funcion_lenta)
      ocupar_cpu(10);
    en_funcion = 0;
  }

  sigreturn();
  exit(1);
}

static int
periodicidad(void)
{
  return sigalarm(5, atender_alarma) == 0 && esperar_alarmas(3);
}

static int
proceso_dormido(void)
{
  if (sigalarm(10, atender_alarma) < 0 || pause(25) < 0)
    return 0;

  ocupar_cpu(3);
  return alarmas == 0 && esperar_alarmas(1);
}

static int
restaurar_registro(void)
{
  uint64 valor, entregada;

  if (sigalarm(1, atender_alarma) < 0)
    return 0;

  // Mantener un valor conocido en a0 hasta recibir una alarma.
  asm volatile(
      "li a0, 123\n\t"
      "sw zero, 0(%2)\n\t"
      "li t0, 50000000\n\t"
      "1: lw t1, 0(%2)\n\t"
      "bnez t1, 2f\n\t"
      "addi t0, t0, -1\n\t"
      "bnez t0, 1b\n\t"
      "2: mv %0, a0\n\t"
      "mv %1, t1"
      : "=&r"(valor), "=&r"(entregada)
      : "r"(&alarmas)
      : "a0", "t0", "t1", "memory");

  return entregada != 0 && valor == 123;
}

static int
procesos_independientes(void)
{
  int estado;

  if (sigalarm(2, atender_alarma) < 0)
    return 0;

  int pid = fork();
  if (pid < 0)
    return 0;

  if (pid == 0) {
    // El hijo debe empezar sin el temporizador del padre.
    alarmas = 0;
    ocupar_cpu(12);
    if (alarmas != 0 || sigalarm(7, atender_alarma) < 0)
      exit(1);
    exit(!esperar_alarmas(2));
  }

  int padre_ok = esperar_alarmas(2);
  sigalarm(0, 0);
  return wait(&estado) == pid && estado == 0 && padre_ok;
}

static int
desactivacion(void)
{
  if (sigalarm(3, atender_alarma) < 0 || !esperar_alarmas(1))
    return 0;

  if (sigalarm(0, 0) < 0)
    return 0;

  int antes = alarmas;
  ocupar_cpu(20);
  return alarmas == antes;
}

static int
sin_anidamiento(void)
{
  funcion_lenta = 1;
  return sigalarm(2, atender_alarma) == 0 &&
         esperar_alarmas(2) && !anidada;
}

static int
argumentos_invalidos(void)
{
  return sigalarm(-1, atender_alarma) == -1 && sigreturn() == -1;
}

static volatile int iniciar_exec;

static void
alarma_exec(void)
{
  if (iniciar_exec) {
    char *args[] = {"alarmas_test", "--despues-de-exec", 0};
    exec("/alarmas_test", args);
    exit(1);
  }

  // Si el temporizador anterior sigue activo, quedará registrado.
  alarmas++;
  sigreturn();
  exit(1);
}

static int
reinicio_en_exec(void)
{
  iniciar_exec = 1;

  if (sigalarm(2, alarma_exec) < 0)
    return 0;

  ocupar_cpu(30);
  return 0;
}

static int
probar(const char *nombre, int (*prueba)(void))
{
  int estado;
  int pid = fork();

  if (pid == 0)
    exit(!prueba());

  int ok = pid > 0 && wait(&estado) == pid && estado == 0;
  printf("%s: %s\n", nombre, ok ? "OK" : "FALLO");
  return ok;
}

int
main(int argc, char **argv)
{
  if (argc == 2 && strcmp(argv[1], "--despues-de-exec") == 0) {
 	 // El programa nuevo debe empezar sin una alarma en curso.
  	if (sigreturn() != -1)
    	exit(1);

  	// Tampoco debe recibir avisos del temporizador anterior.
  	ocupar_cpu(15);
  	exit(alarmas != 0);
  }
 
  int aprobadas = 0;

  aprobadas += probar("P1 Periodicidad", periodicidad);
  aprobadas += probar("P2 Proceso dormido", proceso_dormido);
  aprobadas += probar("P3 Restauracion de a0", restaurar_registro);
  aprobadas += probar("P4 Procesos independientes", procesos_independientes);
  aprobadas += probar("P5 Desactivacion", desactivacion);
  aprobadas += probar("P6 Sin anidamiento", sin_anidamiento);
  aprobadas += probar("P7 Llamadas invalidas", argumentos_invalidos);
  aprobadas += probar("P8 Reinicio con exec", reinicio_en_exec);
  printf("Pruebas aprobadas: %d/8\n", aprobadas);
  exit(aprobadas != 8);
}	

