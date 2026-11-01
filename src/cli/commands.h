#ifndef FUSE_COMMANDS_H
#define FUSE_COMMANDS_H

/* Capability commands receive their subcommand name as argv[0]. */
int fuse_gl_main(int argc, char **argv);
int fuse_tex_main(int argc, char **argv);
int fuse_mesh_main(int argc, char **argv);

#endif
