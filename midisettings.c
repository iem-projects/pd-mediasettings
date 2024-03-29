/******************************************************
 *
 * midisettings - get/set midi preferences from within Pd-patches
 * Copyright (C) 2010-2019 IOhannes m zmölnig
 *
 *   forum::für::umläute
 *
 *   institute of electronic music and acoustics (iem)
 *   university of music and dramatic arts, graz (kug)
 *
 *
 ******************************************************
 *
 * license: GNU General Public License v.3 or later
 *
 ******************************************************/
#include "mediasettings.h"

#if (!defined MIDISETTINGS_VERSION) && (defined VERSION)
# define MIDISETTINGS_VERSION VERSION
#endif

#ifndef MAXMIDIINDEV
# define MAXMIDIINDEV 4
#endif
#ifndef MAXMIDIOUTDEV
# define MAXMIDIOUTDEV 4
#endif

#if MAXMIDIINDEV > MAXMIDIOUTDEV
# define MAXMIDIDEV MAXMIDIINDEV
#else
# define MAXMIDIDEV MAXMIDIOUTDEV
#endif

extern int sys_midiapi;
static t_class *midisettings_class;

static t_symbol*s_pdsym=NULL;

typedef struct _ms_symkeys {
  t_symbol*name;
  int      id;

  struct _ms_symkeys *next;
} t_ms_symkeys;

typedef t_ms_symkeys t_ms_drivers ;



static void ms_symkeys_print(t_ms_symkeys*symkeys) {
  while(symkeys) {
    post("symkey[%s]=%d", (symkeys->name)?symkeys->name->s_name:"<nil>", symkeys->id);
    symkeys=symkeys->next;
  }
}


static const char*ms_defaultdrivername(const int id) {
  switch (id) {
  case API_NONE:
    return NULL;
  case API_ALSA:
    return "ALSA-MIDI";
  default:
    return "default-MIDI"; /* such a stupid name! */
  }
  return NULL;
}

static t_ms_symkeys*ms_symkeys_find(t_ms_symkeys*symkeys, const t_symbol*name) {
  while(symkeys) {
    if(name==symkeys->name)return symkeys;
    symkeys=symkeys->next;
  }
  return NULL;
}

static t_ms_symkeys*ms_symkeys_findid(t_ms_symkeys*symkeys, const int id) {
  while(symkeys) {
    if(id==symkeys->id)return symkeys;
    symkeys=symkeys->next;
  }
  return NULL;
}

static t_ms_symkeys*ms_symkeys_add(t_ms_symkeys*symkeys, t_symbol*name, int id, int overwrite) {
  t_ms_symkeys*symkey=ms_symkeys_find(symkeys, name);

  if(symkey) {
    char buf[MAXPDSTRING+1];
    buf[MAXPDSTRING]=0;

    if(!overwrite)
      return symkeys;
#warning LATER check how to deal with multiple devices of the same name!
    // now this is a simple hack
    snprintf(buf, MAXPDSTRING, "%s[%d]", name->s_name, id);
    return ms_symkeys_add(symkeys, gensym(buf), id, overwrite);
  }



  symkey=(t_ms_symkeys*)getbytes(sizeof(t_ms_symkeys));
  symkey->name=name;
  symkey->id=id;
  symkey->next=symkeys;

  return symkey;
}

static void ms_symkeys_clear(t_ms_symkeys*symkeys) {
  t_ms_symkeys*symkey=symkeys;
  while(symkey) {
    t_ms_symkeys*next=symkey->next;

    symkey->name=NULL;
    symkey->id=0;
    symkey->next=NULL;

    freebytes(symkey, sizeof(t_ms_symkeys));

    symkey=next;
  }
}

static t_symbol*ms_symkeys_getname(t_ms_symkeys*symkeys, const int id) {
  t_ms_symkeys*driver=ms_symkeys_findid(symkeys, id);
  if(driver)
    return driver->name;
  return NULL;
}
static int ms_symkeys_getid(t_ms_symkeys*symkeys, const t_symbol*name) {
  t_ms_symkeys*symkey=ms_symkeys_find(symkeys, name);
  if(symkey) {
    return symkey->id;
  }
  return -1; /* unknown */
}


t_ms_symkeys*ms_driverparse(t_ms_symkeys*drivers, const char*buf) {
  int start=-1;
  int stop =-1;

  unsigned int index=0;
  int depth=0;
  const char*s;
  char substring[MAXPDSTRING];

  for(index=0, s=buf; 0!=*s; s++, index++) {
    if('{'==*s) {
      start=index;
      depth++;
    }
    if('}'==*s) {
      depth--;
      stop=index;

      if(start>=0 && start<stop) {
        char drivername[MAXPDSTRING];
        int  driverid;
        int length=stop-start;
        if(length>=MAXPDSTRING)length=MAXPDSTRING-1;
        snprintf(substring, length, "%s", buf+start+1);

        if(common_parsedriver(substring, length,
                              drivername, MAXPDSTRING,
                              &driverid)) {
          drivers=ms_symkeys_add(drivers, gensym(drivername), driverid, 0);
        } else {
          if((start+1)!=(stop)) /* empty APIs string */
            post("unparseable: '%s'", substring);
        }
      }
      start=-1;
      stop=-1;
    }
  }

  return drivers;
}

static t_ms_symkeys*DRIVERS=NULL;

static t_symbol*ms_getdrivername(const int id) {
  t_symbol*s=ms_symkeys_getname(DRIVERS, id);
  if(s)
    return s;
  else {
    const char*name=ms_defaultdrivername(id);
    if(name)
      return gensym(name);
  }
  return gensym("<unknown>");
}

static int ms_getdriverid(const t_symbol*id) {
  return ms_symkeys_getid(DRIVERS, id);
}



typedef struct _ms_params {
  int indev[MAXMIDIINDEV], outdev[MAXMIDIOUTDEV];
  unsigned int num_indev, num_outdev;

  t_ms_symkeys*indevices, *outdevices;
  unsigned int num_indevices, num_outdevices;
} t_ms_params;

static void ms_params_print(t_ms_params*parms, const int terseness) {
  int i=0;
#if 0
  const int maxin =MAXMIDIINDEV;
  const int maxout=MAXMIDIOUTDEV;
#else
  const int maxin =parms->num_indev;
  const int maxout=parms->num_outdev;
#endif
  verbose(terseness, "=================================<");
  if(API_ALSA == sys_midiapi) {
    verbose(terseness, "alsamidi: %d %d", parms->num_indev, parms->num_outdev);
  } else {
    for(i=0; i<maxin; i++) {
      verbose(terseness, "indev[%d]: %d", i, parms->indev[i]);
    }
    for(i=0; i<maxout; i++) {
      verbose(terseness, "outdev[%d]: %d", i, parms->outdev[i]);
    }
  }

  verbose(terseness, ">=================================");
}

static t_ms_symkeys*ms_params_adddevices(
  t_ms_symkeys*keys, unsigned int*number,
  char devlist[MAXNDEV][DEVDESCSIZE], unsigned int numdevs) {
  unsigned int num=0;
  if(number)num=*number;
  unsigned int i;
  for(i=0; i<numdevs; i++) {
    num++;
    keys=ms_symkeys_add(keys, gensym(devlist[i]), num, 1);
  }
  if(number)*number=num;
  return keys;
}

static void ms_params_get(t_ms_params*parms) {
  char indevlist[MAXNDEV][DEVDESCSIZE], outdevlist[MAXNDEV][DEVDESCSIZE];
  int indevs = 0, outdevs = 0;

  ms_symkeys_clear(parms->indevices);
  ms_symkeys_clear(parms->outdevices);
  memset(parms, 0, sizeof(t_ms_params));

  sys_get_midi_devs((char*)indevlist, &indevs,
                    (char*)outdevlist, &outdevs,
                    MAXNDEV, DEVDESCSIZE);

  parms->num_indevices=0;
  parms->indevices=ms_params_adddevices(parms->indevices, &parms->num_indevices, indevlist, indevs);
  parms->num_outdevices=0;
  parms->outdevices=ms_params_adddevices(parms->outdevices, &parms->num_outdevices, outdevlist, outdevs);

  sys_get_midi_params(&indevs , parms->indev,
                      &outdevs, parms->outdev);

  parms->num_indev =(indevs >0)?indevs:0;
  parms->num_outdev=(outdevs>0)?outdevs:0;

  // ms_params_print(parms, 0);
}


typedef struct _midisettings
{
  t_object x_obj;
  t_outlet*x_info;

  t_ms_params x_params;
} t_midisettings;

static void midisettings_params_init(t_midisettings*x) {
  int i=0;
  for(i=0; i<MAXMIDIINDEV ; i++) x->x_params.indev [i]=0;
  for(i=0; i<MAXMIDIOUTDEV; i++) x->x_params.outdev[i]=0;

  x->x_params.num_indev = x->x_params.num_outdev = 0;

  ms_params_get(&x->x_params);
}

static void midisettings_debug(t_midisettings*x) {
  post("IN-DEVS");ms_symkeys_print(x->x_params.indevices);
  post("OUTDEVS");ms_symkeys_print(x->x_params.outdevices);

}

#define MS_ALSADEV_FORMAT "ALSA-%02d"

/* 'device in <devname1> <devname2> ...'
 * 'device out <devnameX> <deviceY> ...'
 */
static int midisettings_listdevices_devices(
  t_atom *atoms, /* MAXMIDIDEV+1 atoms */
  t_symbol*type,
  t_ms_symkeys*devices,
  const unsigned int numdevs
  ) {
  unsigned int count=0, i=0;
  SETSYMBOL(atoms+0, type);
  for(i=0; i<numdevs; i++) {
    char dummy[MAXPDSTRING];
    const char*devname=NULL;
    if(API_ALSA == sys_midiapi) {
      snprintf(dummy, MAXPDSTRING, MS_ALSADEV_FORMAT, i);
      dummy[MAXPDSTRING-1]=0;
      devname=dummy;
    } else {
      t_symbol *s_devname=ms_symkeys_getname(devices, i);
      if(s_devname) {
        devname=s_devname->s_name;
      }
    }
    if(devname) {
      SETSYMBOL(atoms+count+1, gensym(devname));
      count++;
    }
  }
  return count;
}

/* 'devicelist in <numdevices>' + 'devicelist in <devName> <devId>'
 * 'devicelist out <numdevices>' + 'devicelist out <devName> <devId>'
 */
static int midisettings_listdevices_devicelist(
  t_atom*atoms, /* t_atom[MAXMIDIDEV*3] */
  t_symbol*type,
  t_ms_symkeys*devices,
  const unsigned int numdevs,
  const unsigned int maxdevs
  ) {
  unsigned int i=0;
  int count = 0;
  if(API_ALSA == sys_midiapi) {
    char dummy[MAXPDSTRING];
    for(i=0; i<maxdevs; i++) {
      t_atom*curatoms = atoms+3*count;

      snprintf(dummy, MAXPDSTRING, MS_ALSADEV_FORMAT, i);
      dummy[MAXPDSTRING-1]=0;
      SETSYMBOL(curatoms+0, type);
      SETSYMBOL(curatoms+1, gensym(dummy));
      SETFLOAT (curatoms+2, (t_float)i);
      count++;
    }
  } else {
    for(i=0; i<numdevs && devices; i++, devices=devices->next) {
      t_atom*curatoms = atoms + 3*count;
      if(NULL==devices->name)
        continue;
      SETSYMBOL(curatoms+0, type);
      SETSYMBOL(curatoms+1, devices->name);
      SETFLOAT (curatoms+2, (t_float)(devices->id));
      count++;
    }
  }
  return count;
}


static void midisettings_listdevices(t_midisettings *x)
{
  t_atom indevices[MAXMIDIDEV+1], outdevices[MAXMIDIDEV+1];
  t_atom indevlist[MAXMIDIDEV*3], outdevlist[MAXMIDIDEV*3];
  t_atom devlenatoms[2];
  int numindevices = 0, numoutdevices = 0;
  int indevlistlen = 0, outdevlistlen = 0;
  int i=0;

  numindevices = midisettings_listdevices_devices(
    indevices,
    gensym("in"),
    x->x_params.indevices,
    x->x_params.num_indev);

  numoutdevices=midisettings_listdevices_devices(
    outdevices,
    gensym("out"),
    x->x_params.outdevices,
    x->x_params.num_outdev);

  indevlistlen = midisettings_listdevices_devicelist(
    indevlist,
    gensym("in"),
    x->x_params.indevices,
    x->x_params.num_indevices,
    MAXMIDIINDEV);

  outdevlistlen = midisettings_listdevices_devicelist(
    outdevlist,
    gensym("out"),
    x->x_params.outdevices,
    x->x_params.num_outdevices,
    MAXMIDIOUTDEV);


  outlet_anything(x->x_info, gensym("device"), numindevices +1, indevices);
  outlet_anything(x->x_info, gensym("device"), numoutdevices+1, outdevices);
  SETSYMBOL(devlenatoms+0, gensym("in")) ; SETFLOAT(devlenatoms+1,  indevlistlen);
  outlet_anything(x->x_info, gensym("devicelist"), 2, devlenatoms);
  for(i=0; i<indevlistlen; i++) {
    outlet_anything(x->x_info, gensym("devicelist"), 3, indevlist+3*i);
  }
  SETSYMBOL(devlenatoms+0, gensym("out")); SETFLOAT(devlenatoms+1, outdevlistlen);
  outlet_anything(x->x_info, gensym("devicelist"), 2, devlenatoms);
  for(i=0; i<outdevlistlen; i++) {
    outlet_anything(x->x_info, gensym("devicelist"), 3, outdevlist+3*i);
  }
}

static void midisettings_params_apply(t_midisettings*x) {
/*
  "pd midi-dialog ..."
  #00: indev[0]
  #01: indev[1]
  #02: indev[2]
  #03: indev[3]
  #04: outdev[0]
  #05: outdev[1]
  #06: outdev[2]
  #07: outdev[3]
  #08: alsadevin
  #09: alsadevout
*/
#if 0
# define MIDIDIALOG_INDEVS MAXMIDIINDEV
# define MIDIDIALOG_OUTDEVS MAXMIDIOUTDEV
#else
# define MIDIDIALOG_INDEVS 4
# define MIDIDIALOG_OUTDEVS 4
#endif

  int alsamidi=(API_ALSA==sys_midiapi);

  t_atom argv [MIDIDIALOG_INDEVS+MIDIDIALOG_OUTDEVS+2];
  unsigned int    argc= MIDIDIALOG_INDEVS+MIDIDIALOG_OUTDEVS+2;

  unsigned int i=0;

  ms_params_print(&x->x_params, 0);

  for(i=0; i<argc; i++) {
    SETFLOAT(argv+i, (t_float)0);
  }


  if(alsamidi) {
    for(i=0; i<MIDIDIALOG_INDEVS; i++) {
      SETFLOAT(argv+i+0*MIDIDIALOG_INDEVS, (t_float)0);
    }
    for(i=0; i<MIDIDIALOG_OUTDEVS; i++) {
      SETFLOAT(argv+i+1*MIDIDIALOG_INDEVS, (t_float)0);
    }

    SETFLOAT(argv+1*MIDIDIALOG_INDEVS+1*MIDIDIALOG_OUTDEVS+0,(t_float)x->x_params.num_indev );
    SETFLOAT(argv+1*MIDIDIALOG_INDEVS+1*MIDIDIALOG_OUTDEVS+1,(t_float)x->x_params.num_outdev);

  } else {
    unsigned int pos=0;
    for(i=0; i<x->x_params.num_indev; i++) {
      pos=i+0*MIDIDIALOG_INDEVS;
      SETFLOAT(argv+pos, (t_float)x->x_params.indev[i]);
    }
    for(i=0; i<x->x_params.num_indev; i++) {
      pos=i+1*MIDIDIALOG_INDEVS;
      SETFLOAT(argv+pos, (t_float)x->x_params.outdev[i]);
    }
    pos=MIDIDIALOG_INDEVS+MIDIDIALOG_OUTDEVS;
    SETFLOAT(argv+1*MIDIDIALOG_INDEVS+1*MIDIDIALOG_OUTDEVS+0,(t_float)x->x_params.num_indev );
    SETFLOAT(argv+1*MIDIDIALOG_INDEVS+1*MIDIDIALOG_OUTDEVS+1,(t_float)x->x_params.num_outdev);
  }

  if (s_pdsym->s_thing) typedmess(s_pdsym->s_thing,
				  gensym("midi-dialog"),
				  argc,
				  argv);
}


/* find the beginning of the next parameter in the list */
typedef enum {
  PARAM_INPUT,
  PARAM_OUTPUT,
  PARAM_INVALID
} t_paramtype;
static t_paramtype midisettings_setparams_id(t_symbol*s) {
  if(gensym("@input")==s) {
    return PARAM_INPUT;
  } else if(gensym("@output")==s) {
    return PARAM_OUTPUT;
  } else if(gensym("@in")==s) {
    return PARAM_INPUT;
  } else if(gensym("@out")==s) {
    return PARAM_OUTPUT;
  }

  return PARAM_INVALID;
}

/* find the beginning of the next parameter in the list */
static int midisettings_setparams_next(int argc, t_atom*argv) {
  int i=0;
  for(i=0; i<argc; i++) {
    if(A_SYMBOL==argv[i].a_type) {
      t_symbol*s=atom_getsymbol(argv+i);
      if('@'==s->s_name[0])
	return i;
    }
  }
  return i;
}

/* [<device1> [<deviceN>]*] ... */
static int midisettings_setparams_inout(
  int argc, t_atom*argv,
  t_ms_symkeys*devices, int*devicelist, unsigned int*numdevices,
  const unsigned int maxnumdevices ) {
  const unsigned int length=midisettings_setparams_next(argc, argv);
  unsigned int len=length;
  unsigned int i;

  if(len>maxnumdevices)
    len=maxnumdevices;

  *numdevices = len;

  for(i=0; i<len; i++) {
    int dev=0;
    switch(argv[i].a_type) {
    case A_FLOAT:
      dev=atom_getint(argv);
      break;
    case A_SYMBOL:
      // LATER: get the device-id from the device-name
      dev=ms_symkeys_getid(devices, atom_getsymbol(argv+i));
      if(dev<0) dev=0;
      break;
    default:
      continue;
    }
    devicelist[i]=dev;
  }
  return length;
}

static int midisettings_setparams_input(t_midisettings*x, int argc, t_atom*argv) {
  int advance =  midisettings_setparams_inout(
    argc, argv,
    x->x_params.indevices, x->x_params.indev, &x->x_params.num_indev, MAXMIDIINDEV);
  return advance;
}

static int midisettings_setparams_output(t_midisettings*x, int argc, t_atom*argv) {
  return  midisettings_setparams_inout(
    argc, argv,
    x->x_params.outdevices, x->x_params.outdev, &x->x_params.num_outdev, MAXMIDIOUTDEV);
}

static void midisettings_setparams(t_midisettings *x, t_symbol*s, int argc, t_atom*argv) {
/*
 * PLAN:
 *   several messages that accumulate to a certain settings, and then "apply" them
 *
 * normal midi: list up to four devices (each direction)
 * alsa   midi: choose number of ports (each direction)

 */
  int apply=1;
  int advance=0;
  t_paramtype param=PARAM_INVALID;

  enum {
    INPUT, OUTPUT, PARAM
  } type;

  type=PARAM;

  if(!argc) {
    midisettings_listdevices(x);
    return;
  }

  if(A_SYMBOL == argv->a_type ) {
    t_symbol*tsym=atom_getsymbol(argv);
    if(gensym("in")==tsym)
      type=INPUT;
    else if(gensym("out")==tsym)
      type=OUTPUT;

    if(PARAM!=type) {
      argc--;
      argv++;
    }
  }

  midisettings_params_init (x); /* re-initialize to what we got */


  switch(type) {
  case INPUT:
    midisettings_setparams_input(x, argc, argv);
    break;
  case OUTPUT:
    midisettings_setparams_output(x, argc, argv);
    break;
  case PARAM:
    advance=midisettings_setparams_next(argc, argv);
    while((argc-=advance)>0) {
      argv+=advance;
      s=atom_getsymbol(argv);
      param=midisettings_setparams_id(s);

      argv++;
      argc--;

      switch(param) {
      case PARAM_INPUT:
        advance=midisettings_setparams_input(x, argc, argv);
        break;
      case PARAM_OUTPUT:
        advance=midisettings_setparams_output(x, argc, argv);
        break;
      default:
        pd_error(x, "unknown parameter %d:", param); postatom(1, argv);endpost();
        break;
      }

      argc-=advance;
      argv+=advance;
      advance=midisettings_setparams_next(argc, argv);
    }
    break;
  }
  if(apply) {
    midisettings_params_apply (x); /* re-initialize to what we got */
  }
}

static void midisettings_listdrivers(t_midisettings *x);
static void midisettings_setdriver(t_midisettings *x, t_symbol*s, int argc, t_atom*argv) {
  int id=-1;
  s=gensym("<unknown>"); /* just re-use the argument, which is not needed anyhow */
  switch(argc) {
  case 0:
    midisettings_listdrivers(x);
    return;
  case 1:
    if(A_FLOAT==argv->a_type) {
      s=ms_getdrivername(atom_getint(argv));
      break;
    } else if (A_SYMBOL==argv->a_type) {
      s=atom_getsymbol(argv);
      break;
    }
  }

  if(NULL==DRIVERS) {
    id=sys_midiapi;
  } else {
    id=ms_getdriverid(s);
    if(id<0) {
      pd_error(x, "invalid driver '%s'", s->s_name);
      return;
    }
    verbose(1, "setting driver '%s' (=%d)", s->s_name, id);
  }
  sys_close_midi();
  sys_set_midi_api(id);
  sys_reopen_midi();
}

/*
 */
static void midisettings_listdrivers(t_midisettings *x)
{
  t_ms_drivers*driver=NULL;
  t_atom a1[1];
  t_atom*adrivers=0;
  size_t count=0;
  for(driver=DRIVERS; driver; driver=driver->next) {
    count++;
  }
  adrivers=getbytes(sizeof(t_atom) * (count+2));
  if(adrivers) {
    size_t i;
    for(driver=DRIVERS, i=0; driver; driver=driver->next, i++) {
      SETSYMBOL(adrivers+i*2+0, driver->name);
      SETFLOAT (adrivers+i*2+1, (t_float)(driver->id));
    }
  }

  SETSYMBOL(a1+0, ms_getdrivername(sys_midiapi));
  outlet_anything(x->x_info, gensym("driver"), 1, a1);

  SETFLOAT(a1+0, count);
  outlet_anything(x->x_info, gensym("driverlist"), 1, a1);

  if(adrivers) {
    size_t i;
    for(i=0; i<count; i++) {
      outlet_anything(x->x_info, gensym("driverlist"), 2, adrivers+2*i);
    }
    freebytes(adrivers, (sizeof(t_atom) * (count+2)));
  }
}

static void midisettings_bang(t_midisettings *x) {
  midisettings_listdrivers(x);
  midisettings_listdevices(x);
}


static void midisettings_free(t_midisettings *x){
#warning cleanup
  (void)x;
}


static void *midisettings_new(void)
{
  t_midisettings *x = (t_midisettings *)pd_new(midisettings_class);
  x->x_info=outlet_new(&x->x_obj, 0);

  x->x_params.indevices=NULL;
  x->x_params.outdevices=NULL;

  char buf[MAXPDSTRING];
  sys_get_midi_apis(buf);
  DRIVERS=ms_driverparse(DRIVERS, buf);

  midisettings_params_init (x); /* re-initialize to what we got */

  return (x);
}

static void midisettings_testdevices(t_midisettings *x);

void midisettings_setup(void)
{
  s_pdsym=gensym("pd");

  mediasettings_boilerplate("[midisettings] midi settings manager",
#ifdef MIDISETTINGS_VERSION
                            MIDISETTINGS_VERSION
#else
                            0
#endif
    );

  midisettings_class = class_new(gensym("midisettings"),
                                 (t_newmethod)midisettings_new,
                                 (t_method)midisettings_free,
                                 sizeof(t_midisettings),
                                 0, 0);

  class_addbang(midisettings_class, (t_method)midisettings_bang);
  class_addmethod(midisettings_class, (t_method)midisettings_listdrivers, gensym("listdrivers"), A_NULL);
  class_addmethod(midisettings_class, (t_method)midisettings_listdevices, gensym("listdevices"), A_NULL);

  class_addmethod(midisettings_class, (t_method)midisettings_setdriver, gensym("driver"), A_GIMME, A_NULL);
  class_addmethod(midisettings_class, (t_method)midisettings_setparams, gensym("device"), A_GIMME, A_NULL);

  class_addmethod(midisettings_class, (t_method)midisettings_debug, gensym("print"), A_NULL);
}



static void midisettings_testdevices(t_midisettings *x)
{
  int i;

  char indevlist[MAXNDEV][DEVDESCSIZE], outdevlist[MAXNDEV][DEVDESCSIZE];
  int indevs = 0, outdevs = 0;
  (void)x;

  sys_get_midi_devs((char*)indevlist, &indevs, (char*)outdevlist, &outdevs, MAXNDEV, DEVDESCSIZE);

  post("%d midi indevs", indevs);
  for(i=0; i<indevs; i++)
    post("\t#%02d: %s", i, indevlist[i]);

  post("%d midi outdevs", outdevs);
  for(i=0; i<outdevs; i++)
    post("\t#%02d: %s", i, outdevlist[i]);

  endpost();
  int nmidiindev, midiindev[MAXMIDIINDEV];
  int nmidioutdev, midioutdev[MAXMIDIOUTDEV];
  sys_get_midi_params(&nmidiindev, midiindev,
                      &nmidioutdev, midioutdev);

  post("%d midiindev (parms)", nmidiindev);
  for(i=0; i<nmidiindev; i++) {
    post("\t#%02d: %d %d", i, midiindev[i]);
  }
  post("%d midioutdev (parms)", nmidioutdev);
  for(i=0; i<nmidioutdev; i++) {
    post("\t#%02d: %d %d", i, midioutdev[i]);
  }
}
