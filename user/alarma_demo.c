#include "kernel/types.h"
#include "user/user.h"

static volatile int alarmas;

static void
atender_alarma(void)
{
  alarmas++;
  sigreturn();
}

int
main(void)
{
  if (sigalarm(5, atender_alarma) < 0) {
    fprintf(2, "No se pudo configurar la alarma\n");
    exit(1);
  }

  int inicio = uptime();

  while (alarmas < 3 && uptime() - inicio < 50) {
    for (volatile int i = 0; i < 100000; i++) {
      // Mantener al proceso ejecutando en modo usuario.
    }
  }

  sigalarm(0, 0);
  printf("Alarmas recibidas: %d\n", alarmas);
  exit(alarmas < 3);
}
