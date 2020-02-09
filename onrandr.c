#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#define UNPACK(...) __VA_ARGS__

#define container_of(ptr, type, member) \
  ((type*)( (ptr) ? (char*)(ptr) - offsetof(type, member) : 0 ))

enum tf_mode {
  TF_INT,
  TF_SHORT,
  TF_LONG
};

#define EVENT_LIST \
  X(ScreenChange, RRScreenChangeNotify, ( \
    Y(XRRScreenChangeNotifyEvent, timestamp, TF_LONG, "%lu") \
    Y(XRRScreenChangeNotifyEvent, config_timestamp, TF_LONG, "%lu") \
    Y(XRRScreenChangeNotifyEvent, size_index, TF_SHORT, "%hu") \
    Y(XRRScreenChangeNotifyEvent, subpixel_order, TF_SHORT, "%hu") \
    Y(XRRScreenChangeNotifyEvent, rotation, TF_SHORT, "%hu") \
    Y(XRRScreenChangeNotifyEvent, width, TF_INT, "%u") \
    Y(XRRScreenChangeNotifyEvent, height, TF_INT, "%u") \
    Y(XRRScreenChangeNotifyEvent, mwidth, TF_INT, "%u") \
    Y(XRRScreenChangeNotifyEvent, mheight, TF_INT, "%u") \
  )) \
  X(CrtcChange, RRNotify_CrtcChange + 1, ( \
    Y(XRRCrtcChangeNotifyEvent, crtc, TF_LONG, "0x%lx") \
    Y(XRRCrtcChangeNotifyEvent, mode, TF_LONG, "0x%lx") \
    Y(XRRCrtcChangeNotifyEvent, rotation, TF_SHORT, "%hu") \
    Y(XRRCrtcChangeNotifyEvent, x, TF_INT, "%d") \
    Y(XRRCrtcChangeNotifyEvent, y, TF_INT, "%d") \
    Y(XRRCrtcChangeNotifyEvent, width, TF_INT, "%u") \
    Y(XRRCrtcChangeNotifyEvent, height, TF_INT, "%u") \
  )) \
  X(OutputChange, RRNotify_OutputChange + 1, ( \
    Y(XRROutputChangeNotifyEvent, output, TF_LONG, "0x%lx") \
    Y(XRROutputChangeNotifyEvent, crtc, TF_LONG, "0x%lx") \
    Y(XRROutputChangeNotifyEvent, mode, TF_LONG, "0x%lx") \
    Y(XRROutputChangeNotifyEvent, rotation, TF_SHORT, "%hu") \
    Y(XRROutputChangeNotifyEvent, connection, TF_SHORT, "%hu") \
    Y(XRROutputChangeNotifyEvent, subpixel_order, TF_SHORT, "%hu") \
  )) \
  X(OutputProperty, RRNotify_OutputProperty + 1, ( \
    Y(XRROutputPropertyNotifyEvent, output, TF_LONG, "0x%lx") \
    Z(XRROutputPropertyNotifyEvent, property, get_atom) \
    Y(XRROutputPropertyNotifyEvent, timestamp, TF_LONG, "%lu") \
    Y(XRROutputPropertyNotifyEvent, state, TF_INT, "%d") \
  )) \
  X(ProviderChange, RRNotify_ProviderChange + 1, ( \
    Y(XRRProviderChangeNotifyEvent, provider, TF_LONG, "0x%lx") \
    Y(XRRProviderChangeNotifyEvent, timestamp, TF_LONG, "%lu") \
    Y(XRRProviderChangeNotifyEvent, current_role, TF_INT, "%u") \
  )) \
  X(ProviderProperty, RRNotify_ProviderProperty + 1, ( \
    Y(XRRProviderPropertyNotifyEvent, provider, TF_LONG, "0x%lx") \
    Z(XRRProviderPropertyNotifyEvent, property, get_atom) \
    Y(XRRProviderPropertyNotifyEvent, timestamp, TF_LONG, "%lu") \
    Y(XRRProviderPropertyNotifyEvent, state, TF_INT, "%d") \
  )) \
  X(ResourceChange, RRNotify_ResourceChange + 1, ( \
    Y(XRRResourceChangeNotifyEvent, timestamp, TF_LONG, "%lu") \
  ))

static Display* display = 0;

enum event_type {
#define X(A, M, B) E_ ## A,
  EVENT_LIST
#undef X
  EVENT_COUNT
};

struct event_property {
  size_t offset;
  const char* name;
  bool (*callback)(const struct event_property* prop, void* value);
};

struct event_property_fmt {
  struct event_property super;
  enum tf_mode fmt_mod;
  const char* fmt;
};

struct event {
  enum event_type type;
  unsigned long mask_bit;
  const char* name;
  size_t size;
  const struct event_property*const* property;
};

static bool get_fmt(const struct event_property* prop, void* value){
  const struct event_property_fmt* pf = container_of(prop, struct event_property_fmt, super);
  char buf[64] = {0};
  switch(pf->fmt_mod){
    case TF_INT  : snprintf(buf, sizeof(buf), pf->fmt, *(int  *)value); break;
    case TF_SHORT: snprintf(buf, sizeof(buf), pf->fmt, *(short*)value); break;
    case TF_LONG : snprintf(buf, sizeof(buf), pf->fmt, *(long *)value); break;
    default: return false;
  }
  setenv(prop->name, buf, true);
  return true;
}

static bool get_atom(const struct event_property* prop, void* value){
  char* name = XGetAtomName(display, *(Atom*)value);
  setenv(prop->name, name, true);
  XFree(name);
  return true;
}

const struct event event_list[] = {
#define PROP(T, P, F) \
  { \
    .offset = offsetof(T, P), \
    .name = #P, \
    .callback = (F), \
  }
#define Z(T, P, F) &(struct event_property)PROP(T, P, (F)),
#define Y(T, P, M, F) \
  &((struct event_property_fmt){ \
    .super = PROP(T, P, get_fmt), \
    .fmt_mod = (M), \
    .fmt = (F) \
  }).super,
#define X(A, M, B) { \
    .type = E_ ## A, \
    .mask_bit = 1lu<<(M), \
    .name = #A, \
    .size = sizeof((const struct event_property*const[]){ UNPACK B }) / sizeof(struct event_property*), \
    .property =    (const struct event_property*const[]){ UNPACK B }, \
  },
  EVENT_LIST
#undef X
#undef Z
#undef PROP
};

void usage(const char* cmd){
  fprintf(stderr, "Usage: %s [event]... -- command...\n", cmd);
  fprintf(stderr, "Events:");
  for(size_t i=0; i<EVENT_COUNT; i++)
    fprintf(stderr, " %s", event_list[i].name);
  puts("");
}

int main(int argc, char* argv[]){
  char** cmd = 0;
  for(int i=0; i<argc; i++){
    if(strcmp(argv[i], "--"))
      continue;
    argv[i] = 0;
    cmd = &argv[i+1];
    argc = i;
    break;
  }
  unsigned long mask = ~0;
  if(argc >= 2){
    mask = 0;
    for(int i=1; i<argc; i++){
      unsigned long mask_bit = 0;
      for(size_t j=0; j<EVENT_COUNT; j++){
        if(strcmp(event_list[j].name, argv[i]))
          continue;
        mask_bit = event_list[j].mask_bit;
        break;
      }
      if(!mask_bit){
        usage(argv[0]);
        return 1;
      }
      mask |= mask_bit;
    }
  }
  if(!cmd || !*cmd){
    usage(argv[0]);
    return 1;
  }
  display = XOpenDisplay(0);
  if(!display){
    fprintf(stderr, "Failed to open X display %s\n", XDisplayName(0));
    return 1;
  }
  Window root = DefaultRootWindow(display);
  if(!root){
    fprintf(stderr, "DefaultRootWindow failed\n");
    return 1;
  }
  int event_base, error_base;
  int major=1, minor=2;
  if( !XRRQueryExtension(display, &event_base, &error_base)
   || !XRRQueryVersion(display, &major, &minor)
  ){
    fprintf(stderr, "X RandR extension not available\n");
    return 1;
  }
  if(major < 1 || (major == 1 && minor < 2)){
    printf("Server does not support XRandR 1.2 or newer\n");
    return 1;
  }
  XRRSelectInput(display, root, mask);
  while(true){
    XEvent event;
    XNextEvent(display, &event);
    XRRUpdateConfiguration(&event);
    if(event.type < event_base)
      continue;
    long ptype = event.type - event_base;
    enum event_type type = EVENT_COUNT;
    switch(ptype){
        case RRScreenChangeNotify   : type = E_ScreenChange  ; break;
      case RRNotify: switch(((XRRNotifyEvent*)&event)->subtype){
        case RRNotify_CrtcChange    : type = E_CrtcChange    ; break;
        case RRNotify_OutputChange  : type = E_OutputChange  ; break;
        case RRNotify_OutputProperty: type = E_OutputProperty; break;
        case RRNotify_ProviderChange: type = E_ProviderChange; break;
        case RRNotify_ResourceChange: type = E_ResourceChange; break;
      } break;
    }
    if(type >= EVENT_COUNT)
      continue;
    pid_t ret = fork();
    if(ret == -1)
      continue;
    if(ret){
      int wstatus = 0;
      do {
        pid_t w = waitpid(ret, &wstatus, WUNTRACED | WCONTINUED);
        if(w == -1){
            perror("waitpid");
            break;
        }
      } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
      continue;
    }
    const struct event*const e = &event_list[type];
    setenv("type", e->name, true);
    for(size_t i=0; i<e->size; i++){
      const struct event_property* property = e->property[i];
      property->callback(property, ((char*)&event)+property->offset);
    }
    execvp(cmd[0], cmd);
    abort();
  }
  return 0;
}
