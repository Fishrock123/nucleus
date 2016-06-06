#include "duv.h"
#include "utils.h"
#include "loop.h"
#include "handle.h"
#include "timer.h"
#include "stream.h"
#include "tcp.h"

static const duk_function_list_entry duv_handle_methods[] = {
  {"inspect", duv_tostring, 0},
  {"toString", duv_tostring, 0},
  {"close", duv_close, 1},
  {"isActive", duv_is_active, 0},
  {"isClosing", duv_is_closing, 0},
  {"ref", duv_ref, 0},
  {"unref", duv_unref, 0},
  {"hasRef", duv_has_ref, 0},
  {0,0,0}
};

static const duk_function_list_entry duv_timer_methods[] = {
  {"start", duv_timer_start, 3},
  {"stop", duv_timer_stop, 0},
  {"again", duv_timer_again, 0},
  {"setRepeat", duv_timer_set_repeat, 1},
  {"getRepeat", duv_timer_get_repeat, 0},
  {0,0,0}
};

static const duk_function_list_entry duv_stream_methods[] = {
  {"shutdown", duv_shutdown, 1},
  {"listen", duv_listen, 2},
  {"accept", duv_accept, 1},
  {"readStart", duv_read_start, 1},
  {"readStop", duv_read_stop, 0},
  {"write", duv_write, 2},
  {"isReadable", duv_is_readable, 0},
  {"isWritable", duv_is_writable, 0},
  {"setBlocking", duv_stream_set_blocking, 1},
  {0,0,0}
};

static const duk_function_list_entry duv_tcp_methods[] = {
  {"open", duv_tcp_open, 1},
  {"nodelay", duv_tcp_nodelay, 1},
  {"keepalive", duv_tcp_keepalive, 2},
  {"simultaneousAccepts", duv_tcp_simultaneous_accepts, 1},
  {"bind", duv_tcp_bind, 2},
  {"getpeername", duv_tcp_getpeername, 0},
  {"getsockname", duv_tcp_getsockname, 0},
  {"connect", duv_tcp_connect, 3},
  {0,0,0}
};

// // req.c
// {"cancel", duv_cancel, 1},

static const duk_function_list_entry duv_funcs[] = {
  // loop.c
  {"run", duv_run, 0},
  {"walk", duv_walk, 1},

  // libuv handle constructors
  {"Timer", duv_timer, 0},
  {"Tcp", duv_tcp, 0},

  // // pipe.c
  // {"new_pipe", duv_new_pipe, 1},
  // {"pipe_open", duv_pipe_open, 2},
  // {"pipe_bind", duv_pipe_bind, 2},
  // {"pipe_connect", duv_pipe_connect, 3},
  // {"pipe_getsockname", duv_pipe_getsockname, 1},
  // {"pipe_pending_instances", duv_pipe_pending_instances, 2},
  // {"pipe_pending_count", duv_pipe_pending_count, 1},
  // {"pipe_pending_type", duv_pipe_pending_type, 1},
  //
  // // tty.c
  // {"new_tty", duv_new_tty, 2},
  // {"tty_set_mode", duv_tty_set_mode, 2},
  // {"tty_reset_mode", duv_tty_reset_mode, 0},
  // {"tty_get_winsize", duv_tty_get_winsize, 1},
  //
  // // fs.c
  // {"fs_close", duv_fs_close, 2},
  // {"fs_open", duv_fs_open, 4},
  // {"fs_read", duv_fs_read, 4},
  // {"fs_unlink", duv_fs_unlink, 2},
  // {"fs_write", duv_fs_write, 4},
  // {"fs_mkdir", duv_fs_mkdir, 3},
  // {"fs_mkdtemp", duv_fs_mkdtemp, 2},
  // {"fs_rmdir", duv_fs_rmdir, 2},
  // {"fs_scandir", duv_fs_scandir, 2},
  // {"fs_scandir_next", duv_fs_scandir_next, 1},
  // {"fs_stat", duv_fs_stat, 2},
  // {"fs_fstat", duv_fs_fstat, 2},
  // {"fs_lstat", duv_fs_lstat, 2},
  // {"fs_rename", duv_fs_rename, 3},
  // {"fs_fsync", duv_fs_fsync, 2},
  // {"fs_fdatasync", duv_fs_fdatasync, 2},
  // {"fs_ftruncate", duv_fs_ftruncate, 3},
  // {"fs_sendfile", duv_fs_sendfile, 5},
  // {"fs_access", duv_fs_access, 3},
  // {"fs_chmod", duv_fs_chmod, 3},
  // {"fs_fchmod", duv_fs_fchmod, 3},
  // {"fs_utime", duv_fs_utime, 4},
  // {"fs_futime", duv_fs_futime, 4},
  // {"fs_link", duv_fs_link, 3},
  // {"fs_symlink", duv_fs_symlink, 4},
  // {"fs_readlink", duv_fs_readlink, 2},
  // {"fs_chown", duv_fs_chown, 4},
  // {"fs_fchown", duv_fs_fchown, 4},
  //
  // // misc.c
  // {"guess_handle", duv_guess_handle, 1},
  // {"version", duv_version, 0},
  // {"version_string", duv_version_string, 0},
  // {"get_process_title", duv_get_process_title, 0},
  // {"set_process_title", duv_set_process_title, 1},
  // {"resident_set_memory", duv_resident_set_memory, 0},
  // {"uptime", duv_uptime, 0},
  // {"getrusage", duv_getrusage, 0},
  // {"cpu_info", duv_cpu_info, 0},
  // {"interface_addresses", duv_interface_addresses, 0},
  // {"loadavg", duv_loadavg, 0},
  // {"exepath", duv_exepath, 0},
  // {"cwd", duv_cwd, 0},
  // {"os_homedir", duv_os_homedir, 0},
  // {"chdir", duv_chdir, 1},
  // {"get_total_memory", duv_get_total_memory, 0},
  // {"hrtime", duv_hrtime, 0},
  // {"update_time", duv_update_time, 0},
  // {"now", duv_now, 0},
  // {"argv", duv_argv, 0},
  //
  // // miniz.c
  // {"inflate", duv_tinfl, 2},
  // {"deflate", duv_tdefl, 2},

  {NULL, NULL, 0},
};

duk_ret_t duv_push_module(duk_context *ctx) {

  // stack: nucleus

  // uv
  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, duv_funcs);

  // stack: nucleus uv

  // uv.Handle.prototype
  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, duv_handle_methods);

  // stack: nucleus uv Handle.prototype

  // uv.Timer.prototype
  duk_get_prop_string(ctx, -2, "Timer");
  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, duv_timer_methods);
  duk_dup(ctx, -3);
  duk_set_prototype(ctx, -2);
  duk_put_prop_string(ctx, -2, "prototype");
  duk_pop(ctx);

  // stack: nucleus uv Handle.prototype

  // uv.Stream.prototype
  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, duv_stream_methods);
  duk_dup(ctx, -2);
  duk_set_prototype(ctx, -2);

  // stack: nucleus uv Handle.prototype Stream.prototype

  // uv.Tcp.prototype
  duk_get_prop_string(ctx, -3, "Tcp");
  // stack: nucleus uv Handle.prototype Stream.prototype Tcp
  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, duv_tcp_methods);
  // stack: nucleus uv Handle.prototype Stream.prototype Tcp Tcp.prototype
  duk_dup(ctx, -3);
  // stack: nucleus uv Handle.prototype Stream.prototype Tcp Tcp.prototype Stream.prototype
  duk_set_prototype(ctx, -2);
  // stack: nucleus uv Handle.prototype Stream.prototype Tcp Tcp.prototype
  duk_put_prop_string(ctx, -2, "prototype");
  // stack: nucleus uv Handle.prototype Stream.prototype Tcp
  duk_pop(ctx);

  // stack: nucleus uv Handle.prototype Stream.prototype

  duk_pop_2(ctx);

  // stack: nucleus uv

  return 1;
}
