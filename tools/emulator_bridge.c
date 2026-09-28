// Minimal headless mGBA adapter. Tests send ordinary joypad input and only read
// game memory; no state injection or modifications to the ROM under test.
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static struct mCore* core;
static color_t pixels[240*160];
static int error_lines;
static void test_log(struct mLogger* logger, int category, enum mLogLevel level,
                     const char* format, va_list args) {
    (void)logger; (void)category;
    if(level & (mLOG_FATAL | mLOG_ERROR)) {
        if(error_lines++<32) { vfprintf(stderr,format,args); fputc('\n',stderr); }
        else if(error_lines==33) fputs("Further emulator error messages suppressed.\n",stderr);
    }
}
static struct mLogger logger={.log=test_log,.filter=NULL};

static int open_core(const char* path,const char* save_path) {
    error_lines=0;
    mLogSetDefaultLogger(&logger);
    core=mCoreFind(path);
    if(!core || !core->init(core)) return 0;
    mCoreInitConfig(core,"dustline-tests");
    core->setVideoBuffer(core,pixels,240);
    core->setAudioBufferSize(core,1024);
    if(!mCoreLoadFile(core,path)) return 0;
    if(save_path && !mCoreLoadSaveFile(core,save_path,0)) return 0;
    core->reset(core);
    return 1;
}
int emulator_open(const char* path) { return open_core(path,NULL); }
int emulator_open_with_save(const char* path,const char* save_path) {
    return open_core(path,save_path);
}
int emulator_load_save(const char* path) {
    return core && mCoreLoadSaveFile(core,path,0);
}
size_t emulator_state_size(void) { return core ? core->stateSize(core) : 0; }
int emulator_save_state(void* state,size_t size) {
    return core && state && size>=core->stateSize(core) && core->saveState(core,state);
}
int emulator_load_state(const void* state,size_t size) {
    return core && state && size>=core->stateSize(core) && core->loadState(core,state);
}
void emulator_step(int keys,int frames) {
    core->setKeys(core,keys);
    for(int i=0;i<frames;++i) core->runFrame(core);
}
uint32_t emulator_read(uint32_t address) { return core->busRead32(core,address); }
void* emulator_pixels(void) { return pixels; }
int emulator_pixel_size(void) { return sizeof(color_t); }
void emulator_reset(void) { if(core) core->reset(core); }
void emulator_close(void) {
    if(core) { mCoreConfigDeinit(&core->config); core->deinit(core); core=NULL; }
}
