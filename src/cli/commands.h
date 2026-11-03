#ifndef GRAFIC_COMMANDS_H
#define GRAFIC_COMMANDS_H

/* Capability commands receive their subcommand name as argv[0]. */
int grafic_gl_main(int argc, char **argv);
int grafic_tex_main(int argc, char **argv);
int grafic_mesh_main(int argc, char **argv);
int grafic_view_main(int argc, char **argv);

#endif
