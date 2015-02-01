#####################################################################
### Declaraci\xf3n de macros de los ficheros de codigo fuente,
### objeto, cabecera y ejecutable.
###
### El alumno debe ajustar los valores de estas macros para su programa.
###

# MIS_FUENTES indica los nombres de todos los ficheros .c desarrollados
#   por el alumno.
# OBJETOS_PROPORCIONADOS indica los nombres de todos los ficheros .o
#   proporcionados al alumno. Se puede dejar en blanco si no hay ninguno.
# EJECUTABLE indica el nombre que quiere dar el alumno a su fichero ejecutable.


MIS_FUENTES = fase3.c
OBJETOS_PROPORCIONADOS = http.o param.o
EJECUTABLE = servidor_web

#####################################################################

#####################################################################
### Opciones para el compilador y enlazador.
###
### Probablemente el alumno no necesitara modificarlas.
###

INCLUDES = -I/usr/local/include
BIBLIOTECAS = -L/usr/local/lib -lm
CFLAGS =  $(INCLUDES) -g -Wall -pedantic -Wconversion -Wredundant-decls -Wunreachable-code -O -Wuninitialized -Wformat=2 -Wstrict-prototypes
LDFLAGS = $(BIBLIOTECAS)

#####################################################################


#####################################################################
### Declaraci\xf3n de reglas y dependencias

all: $(EJECUTABLE)

todo: all

limpia: clean
	.sinclude ".depend"

$(EJECUTABLE): $(MIS_FUENTES:C/\.c$/.o/g) $(OBJETOS_PROPORCIONADOS)
	gcc $(CFLAGS) -o $@ $> $(LDFLAGS)

clean:;
	rm -f $(MIS_FUENTES:C/\.c$/.o/g) *core *~ *.bak $(EJECUTABLE)

depend:;
	@gcc -E -MM $(MIS_FUENTES) > .depend
	@echo "Lista de dependencias creada"

#####################################################################
## Uso:
# make  -> Compila y genera el ejecutable si no hay error
# make clean o make limpia -> Borra los ficheros objeto, ejecutable y cores
# make depend -> Genera la lista de dependencias fuente->objeto
#####################################################################

