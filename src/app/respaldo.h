// Respaldo de las partidas al USB y su restauración: guardados/ (baterías, estados, trucos y ajustes de cada juego),
// favoritos, recientes y ajustes, en <USB>/ALLESIU-respaldo. En un hilo aparte.
#pragma once

bool RespaldoEmpezar(bool restaurar);   // false si ya hay uno en curso
// Estado: en curso, terminado (una vez; después false) y el resultado.
bool RespaldoEnCurso();
bool RespaldoTerminado(char* texto, int max, bool* ok);
