# Quita las excepciones de C++ de un archivo de un núcleo (FW 9.00 no abre programas con el runtime de excepciones). Solo
# para los casos simples: "try { ... } catch(...) { ... }" queda como "if (true) { ... } else if (false) { ... }" y
# "throw X(...)" como "(void)X(...)" (el error se ignora: el código sigue como si nada). Lo usa nucleos.sh.
s/\btry\b/if (true)/g
s/catch[ \t]*([ \t]*\.\.\.[ \t]*)/else if (false)/g
s/\bthrow[ \t]\+/(void)/g
# Con tipo ("catch (std::bad_alloc&)"): igual, el bloque nunca corre.
s/catch[ \t]*([^)]*)/else if (false)/g
