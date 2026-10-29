// Direct native test only: compile with GNU --wrap flags listed in the execution receipt.
// No Link engine, private-member hooks, fabricated font pointers or display changes.
#include "SdlRenderer.h"
#include "Config.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
namespace {
struct FontRecord { int generation; std::string path; int size; int closes = 0; };
struct TextUse { int generation; std::string text; SDL_Color color; };
struct Action { std::string kind; int generation; };
std::map<TTF_Font*, int> liveFonts;
std::map<int, FontRecord> records;
std::set<SDL_Window*> windows;
std::set<SDL_Renderer*> renderers;
std::vector<Action> actions;
std::vector<TextUse> textUses;
std::vector<SDL_Color> drawColors;
std::vector<SDL_Rect> fillRects;
std::vector<std::array<int,4>> drawLines;
int nextGeneration = 0, attempts = 0, failAt = 0;
int sdlBalance = 0, ttfBalance = 0, cursorWrites = 0, fullscreenCalls = 0;
int windowCreates = 0, rendererCreates = 0, failures = 0, checks = 0;
SdlRenderer* closeProbe = nullptr;
std::set<int> probeOld;
std::array<int, 4> probeNew{};
bool inProbe = false;
const std::array<std::string,4> fonts = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf"};
const std::array<std::string,4> keys = {
    "status_font_path", "tempo_font_path", "bottom_font_path", "help_font_path"};
const std::array<std::string,4> roleNames = {"status","tempo","bottom","help"};
fs::path fixtureRoot;
void expect(const std::string& name, bool ok) {
    ++checks;
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    if (!ok) ++failures;
}
void must(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
struct Fixtures {
    Fixtures() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i=0;i<100;++i) {
            auto path = fs::temp_directory_path() / ("link-font-tests-"+std::to_string(nonce)+"-"+std::to_string(i));
            if (fs::create_directory(path)) { fixtureRoot=path; break; }
        }
        must(!fixtureRoot.empty(), "Exclusive font fixtures unavailable");
    }
    ~Fixtures() { fs::remove_all(fixtureRoot); }
};
void write(const std::string& text) {
    std::ofstream out(fixtureRoot / "runtime.conf");
    out << text;
    out.close();
    must(static_cast<bool>(out), "Cannot write font config fixture");
}
Config parse(const std::vector<std::string>& extra = {}) {
    std::vector<std::string> args={"font-native-test","--config",(fixtureRoot/"runtime.conf").string(),"--windowed"};
    args.insert(args.end(),extra.begin(),extra.end());
    std::vector<char*> argv;
    for (auto& a:args) argv.push_back(const_cast<char*>(a.c_str()));
    return parseConfig(static_cast<int>(argv.size()),argv.data());
}
std::string roleConfig(bool changed = false) {
    std::string result="font_path="+fonts[0]+"\n";
    for (int i=0;i<4;++i) result+=keys[i]+"="+fonts[changed ? 3-i : i]+"\n";
    result+=changed ?
        "status_font_size=31\ntempo_font_size=81\nbottom_font_size=19\nhelp_font_size=17\n"
        "width=800\nheight=600\nfullscreen=true\nhide_mouse_cursor=false\n"
        "help_overlay_seconds=1\nphase_bar_height=15\nphase_bar_margin=30\n"
        "color_presets=fresh,other\ncolor_preset=other\ncolor_preset.fresh.name=Fresh\n"
        "color_preset.fresh.tempo_color=60,61,62,255\ncolor_preset.other.name=Other\n"
        "color_preset.other.tempo_color=70,71,72,255\n"
        "top_band_color=90,91,92,255\nphase_bar_color=80,81,82,255\n" :
        "hide_mouse_cursor=true\ncolor_presets=base,alt,third\ncolor_preset=base\n"
        "color_preset.base.name=Base\ncolor_preset.alt.name=Alt\ncolor_preset.alt.tempo_color=20,21,22,255\n"
        "color_preset.third.name=Third\ncolor_preset.third.tempo_color=30,31,32,255\n";
    return result;
}
std::set<int> liveIds() {
    std::set<int> result;
    for (const auto& item:liveFonts) result.insert(item.second);
    return result;
}
std::array<int,4> recentFour() {
    return {nextGeneration-3,nextGeneration-2,nextGeneration-1,nextGeneration};
}
bool exactSet(const std::array<int,4>& ids, const std::array<std::string,4>& paths,
              const std::array<int,4>& sizes) {
    if (liveIds()!=std::set<int>(ids.begin(),ids.end())) return false;
    for (int i=0;i<4;++i) {
        const auto& r=records.at(ids[i]);
        if (r.path!=paths[i] || r.size!=sizes[i] || r.closes!=0) return false;
    }
    return true;
}
bool color(SDL_Color c,int r,int g,int b,int a=255) {
    return c.r==r && c.g==g && c.b==b && c.a==a;
}
void observe(SdlRenderer& renderer) {
    textUses.clear(); drawColors.clear(); fillRects.clear(); drawLines.clear();
    LinkDisplayState state;
    renderer.render(state);
}
bool usesRoles(const std::array<int,4>& ids) {
    if (textUses.size()<3 || textUses[0].generation!=ids[0] || textUses[1].generation!=ids[1]) return false;
    for (size_t i=2;i<textUses.size();++i)
        if (textUses[i].generation!=ids[3]) return false;
    // Bottom is owned but has no visible text in the existing phase-only band.
    for (const auto& use:textUses) if (use.generation==ids[2]) return false;
    return true;
}
bool key(SdlRenderer& renderer, SDL_Keycode value) {
    SDL_Event event{};
    event.type=SDL_KEYDOWN;
    event.key.keysym.sym=value;
    must(SDL_PushEvent(&event)==1,"Real SDL event enqueue failed");
    return renderer.pollQuit();
}
struct Capture {
    std::ostringstream output;
    std::streambuf* out=std::cout.rdbuf(output.rdbuf());
    std::streambuf* err=std::cerr.rdbuf(output.rdbuf());
    ~Capture() { std::cout.rdbuf(out); std::cerr.rdbuf(err); }
};
std::string reload(SdlRenderer& renderer) {
    std::string output;
    { Capture c; must(!key(renderer,SDLK_r),"R unexpectedly quit"); output=c.output.str(); }
    std::cout << "RELOAD_LOG_BEGIN\n" << output << "RELOAD_LOG_END\n";
    return output;
}
size_t occurrences(const std::string& haystack,const std::string& needle) {
    size_t count=0,pos=0;
    while ((pos=haystack.find(needle,pos))!=std::string::npos) { ++count; pos+=needle.size(); }
    return count;
}
void cleanBaselineLeaks();
}

extern "C" {
TTF_Font* __real_TTF_OpenFont(const char*,int);
void __real_TTF_CloseFont(TTF_Font*);
SDL_Surface* __real_TTF_RenderUTF8_Blended(TTF_Font*,const char*,SDL_Color);
int __real_SDL_Init(Uint32);
void __real_SDL_Quit();
int __real_TTF_Init();
void __real_TTF_Quit();
SDL_Window* __real_SDL_CreateWindow(const char*,int,int,int,int,Uint32);
void __real_SDL_DestroyWindow(SDL_Window*);
SDL_Renderer* __real_SDL_CreateRenderer(SDL_Window*,int,Uint32);
void __real_SDL_DestroyRenderer(SDL_Renderer*);
int __real_SDL_ShowCursor(int);
int __real_SDL_SetWindowFullscreen(SDL_Window*,Uint32);
int __real_SDL_SetRenderDrawColor(SDL_Renderer*,Uint8,Uint8,Uint8,Uint8);
int __real_SDL_RenderFillRect(SDL_Renderer*,const SDL_Rect*);
int __real_SDL_RenderDrawLine(SDL_Renderer*,int,int,int,int);
TTF_Font* __wrap_TTF_OpenFont(const char* path,int size) {
    ++attempts;
    std::cout << "TRACE_ATTEMPT boundary=" << attempts << " path=" << path << " size=" << size << '\n';
    actions.push_back({"attempt",0});
    if (failAt && attempts==failAt) { SDL_SetError("native harness injected open boundary %d",failAt); return nullptr; }
    auto* font=__real_TTF_OpenFont(path,size);
    if (font) {
        must(!liveFonts.count(font),"Real open returned an already-live pointer");
        const int id=++nextGeneration;
        liveFonts[font]=id;
        records.emplace(id,FontRecord{id,path,size,0});
        actions.push_back({"open",id});
        std::cout << "TRACE_OPEN generation=" << id << " real_pointer=" << static_cast<void*>(font)
                  << " path=" << path << " size=" << size << '\n';
    }
    return font;
}
void __wrap_TTF_CloseFont(TTF_Font* font) {
    const auto found=liveFonts.find(font);
    must(found!=liveFonts.end(),"Unknown or double-closed native font");
    const int id=found->second;
    if (closeProbe && probeOld.count(id) && !inProbe) {
        inProbe=true;
        observe(*closeProbe);
        expect("atomic_live_swap_before_old_close_"+std::to_string(id),usesRoles(probeNew));
        inProbe=false;
    }
    ++records.at(id).closes;
    std::cout << "TRACE_CLOSE generation=" << id << " real_pointer=" << static_cast<void*>(font) << '\n';
    actions.push_back({"close",id});
    liveFonts.erase(font);
    __real_TTF_CloseFont(font);
}
SDL_Surface* __wrap_TTF_RenderUTF8_Blended(TTF_Font* font,const char* text,SDL_Color c) {
    must(liveFonts.count(font),"Render attempted closed or foreign font");
    textUses.push_back({liveFonts.at(font),text,c});
    return __real_TTF_RenderUTF8_Blended(font,text,c);
}
int __wrap_SDL_Init(Uint32 flags) { const int r=__real_SDL_Init(flags); if (!r) ++sdlBalance; return r; }
void __wrap_SDL_Quit() { actions.push_back({"SDL_Quit",0}); --sdlBalance; __real_SDL_Quit(); }
int __wrap_TTF_Init() { const int r=__real_TTF_Init(); if (!r) ++ttfBalance; return r; }
void __wrap_TTF_Quit() { expect("all_fonts_closed_before_TTF_Quit",liveFonts.empty()); actions.push_back({"TTF_Quit",0}); --ttfBalance; __real_TTF_Quit(); }
SDL_Window* __wrap_SDL_CreateWindow(const char* t,int x,int y,int w,int h,Uint32 flags) {
    auto* p=__real_SDL_CreateWindow(t,x,y,w,h,flags); if (p) { windows.insert(p); ++windowCreates; } return p;
}
void __wrap_SDL_DestroyWindow(SDL_Window* p) { must(windows.erase(p)==1,"Window destroy mismatch"); actions.push_back({"window-close",0}); __real_SDL_DestroyWindow(p); }
SDL_Renderer* __wrap_SDL_CreateRenderer(SDL_Window* w,int i,Uint32 flags) {
    auto* p=__real_SDL_CreateRenderer(w,i,flags); if (p) { renderers.insert(p); ++rendererCreates; } return p;
}
void __wrap_SDL_DestroyRenderer(SDL_Renderer* p) { must(renderers.erase(p)==1,"Renderer destroy mismatch"); actions.push_back({"renderer-close",0}); __real_SDL_DestroyRenderer(p); }
int __wrap_SDL_ShowCursor(int value) { if (value!=SDL_QUERY) ++cursorWrites; return __real_SDL_ShowCursor(value); }
int __wrap_SDL_SetWindowFullscreen(SDL_Window* window,Uint32 flags) { ++fullscreenCalls; return __real_SDL_SetWindowFullscreen(window,flags); }
int __wrap_SDL_RenderFillRect(SDL_Renderer* p,const SDL_Rect* rect) {
    fillRects.push_back(rect ? *rect : SDL_Rect{}); return __real_SDL_RenderFillRect(p,rect);
}
int __wrap_SDL_RenderDrawLine(SDL_Renderer* p,int x1,int y1,int x2,int y2) {
    drawLines.push_back({x1,y1,x2,y2}); return __real_SDL_RenderDrawLine(p,x1,y1,x2,y2);
}
int __wrap_SDL_SetRenderDrawColor(SDL_Renderer* p,Uint8 r,Uint8 g,Uint8 b,Uint8 a) {
    drawColors.push_back({r,g,b,a}); return __real_SDL_SetRenderDrawColor(p,r,g,b,a);
}
}
namespace {
void cleanBaselineLeaks() {
    // Recovery is only for leaked resources of THIS native harness, after failing assertions.
    // It is never counted as successful production cleanup.
    if (!liveFonts.empty() || !renderers.empty() || !windows.empty() || ttfBalance || sdlBalance)
        std::cout << "HARNESS_RECOVERY of proven harness-only resources\n";
    while (!liveFonts.empty()) __wrap_TTF_CloseFont(liveFonts.begin()->first);
    while (!renderers.empty()) __wrap_SDL_DestroyRenderer(*renderers.begin());
    while (!windows.empty()) __wrap_SDL_DestroyWindow(*windows.begin());
    while (ttfBalance>0) __wrap_TTF_Quit();
    while (sdlBalance>0) __wrap_SDL_Quit();
}
void startupFailures() {
    for (int n=1;n<=4;++n) {
        write(roleConfig());
        Config c=parse();
        c.fullscreen=true;
        attempts=0; failAt=n; actions.clear();
        const int first=nextGeneration, cursors=cursorWrites;
        std::string error;
        try { SdlRenderer renderer(c); } catch (const std::exception& e) { error=e.what(); }
        failAt=0;
        std::cout << "STARTUP_ERROR " << error << '\n';
        const std::string prefix="startup_boundary_"+std::to_string(n);
        expect(prefix+"_role_path_native_error",error.find("font open unsuccessful")!=std::string::npos
               && error.find("role="+roleNames[n-1])!=std::string::npos
               && error.find(fonts[n-1])!=std::string::npos
               && error.find("native harness injected")!=std::string::npos);
        expect(prefix+"_immediate_sequential_stop",attempts==n && nextGeneration-first==n-1);
        bool closes=true;
        for (int id=first+1;id<=nextGeneration;++id) closes=closes && records.at(id).closes==1;
        expect(prefix+"_each_staged_close_exactly_once",closes && liveFonts.empty());
        expect(prefix+"_window_renderer_init_cleanup",windows.empty() && renderers.empty()
               && sdlBalance==0 && ttfBalance==0 && SDL_WasInit(0)==0 && TTF_WasInit()==0);
        expect(prefix+"_no_cursor_side_effect",cursorWrites==cursors);
        std::vector<std::string> tail;
        for (const auto& action:actions)
            if (action.kind!="open" && action.kind!="attempt" && action.kind!="close") tail.push_back(action.kind);
        expect(prefix+"_cleanup_order",tail==std::vector<std::string>({"renderer-close","window-close","TTF_Quit","SDL_Quit"}));
        cleanBaselineLeaks();
    }
}
void inheritanceAndPaths() {
    std::vector<std::string> configs={"# discovery\n","font_path="+fonts[1]+"\n",roleConfig()};
    for (int i=0;i<4;++i) configs.push_back(keys[i]+"="+fonts[3]+"\n");
    for (size_t n=0;n<configs.size();++n) {
        write(configs[n]); attempts=0;
        Config c=parse();
        std::array<std::string,4> paths;
        paths.fill(c.fontPath);
        if (n==2) paths=fonts;
        if (n>=3) paths[n-3]=fonts[3];
        {
            SdlRenderer renderer(c);
            const auto ids=recentFour();
            expect("native_inheritance_"+std::to_string(n),exactSet(ids,paths,{30,110,25,20}) && attempts==4);
            key(renderer,SDLK_F1);
            observe(renderer);
            expect("real_role_render_"+std::to_string(n),usesRoles(ids));
        }
        expect("success_destruction_"+std::to_string(n),liveFonts.empty() && windows.empty()
               && renderers.empty() && sdlBalance==0 && ttfBalance==0);
    }
    // Explicit nonempty raw paths must reach actual SDL_ttf, with no discovery substitution.
    const auto nonfont=fixtureRoot/"not-font.txt";
    { std::ofstream out(nonfont); out << "not a font\n"; }
    const auto unreadable=fixtureRoot/"unreadable.ttf";
    fs::copy_file(fonts[0],unreadable);
    fs::permissions(unreadable,fs::perms::none);
    for (const auto& bad:std::vector<std::string>{(fixtureRoot/"missing.ttf").string(),fixtureRoot.string(),nonfont.string(),unreadable.string()}) {
        write("font_path="+fonts[0]+"\nhelp_font_path="+bad+"\n");
        attempts=0; std::string error;
        try { SdlRenderer renderer(parse()); } catch (const std::exception& e) { error=e.what(); }
        expect("raw_native_reject_"+bad,error.find("role=help")!=std::string::npos
               && error.find(bad)!=std::string::npos && attempts==4 && liveFonts.empty()
               && windows.empty() && renderers.empty() && !sdlBalance && !ttfBalance);
        cleanBaselineLeaks();
    }
    fs::permissions(unreadable,fs::perms::owner_read | fs::perms::owner_write);
    write("font_path="+(fixtureRoot/"unused-missing.ttf").string()+"\n"+roleConfig().substr(roleConfig().find('\n')+1));
    try {
        SdlRenderer renderer(parse());
        expect("unused_bad_general_not_opened",exactSet(recentFour(),fonts,{30,110,25,20}));
    } catch (const std::exception&) { expect("unused_bad_general_not_opened",false); cleanBaselineLeaks(); }
    // Programmatic missing-discovery equivalent, not a parser/platform discovery claim.
    write(roleConfig()); Config c=parse(); c.fontPath.clear();
    try {
        SdlRenderer renderer(c);
        expect("empty_general_valid_resolved_roles",exactSet(recentFour(),fonts,{30,110,25,20}));
    } catch (const std::exception&) { expect("empty_general_valid_resolved_roles",false); cleanBaselineLeaks(); }
    c.helpFontPath.clear(); attempts=0; std::string error;
    try { SdlRenderer renderer(c); } catch (const std::exception& e) { error=e.what(); }
    expect("empty_missing_role_identified",error.find("role=help")!=std::string::npos
           && error.find("path=<empty>")!=std::string::npos && attempts==3 && liveFonts.empty());
    cleanBaselineLeaks();
}
void reloadBoundaries() {
    for (int n=1;n<=4;++n) {
        write(roleConfig()); attempts=0;
        {
            SdlRenderer renderer(parse());
            const auto old=recentFour(); const auto oldLive=liveIds();
            auto* const window=*windows.begin(); auto* const backend=*renderers.begin();
            const int wc=windowCreates,rc=rendererCreates;
            key(renderer,SDLK_F1);
            expect("F1_event_shows_help",renderer.isHelpOverlayVisible());
            key(renderer,SDLK_f);
            expect("F_event_live_fullscreen",(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0
                   && SDL_ShowCursor(SDL_QUERY)==SDL_DISABLE);
            key(renderer,SDLK_p); observe(renderer);
            expect("P_event_old_alt_color",color(textUses[1].color,20,21,22));
            const auto previousRects=fillRects; const auto previousLines=drawLines;
            int windowW=0,windowH=0; SDL_GetWindowSize(window,&windowW,&windowH);
            write(roleConfig(true)); attempts=0;failAt=n;
            const int first=nextGeneration,cursors=cursorWrites,fc=fullscreenCalls;
            const auto output=reload(renderer); failAt=0;
            const auto prefix="R_boundary_"+std::to_string(n);
            expect(prefix+"_single_failure_no_success",occurrences(output,"font open unsuccessful")==1
                   && output.find("Config reloaded")==std::string::npos
                   && output.find("role="+roleNames[n-1])!=std::string::npos
                   && output.find(fonts[4-n])!=std::string::npos);
            bool closes=liveIds()==oldLive;
            for (int id=first+1;id<=nextGeneration;++id) closes=closes && records.at(id).closes==1;
            for (int id:old) closes=closes && records.at(id).closes==0;
            expect(prefix+"_only_staged_prefix_closed",attempts==n && nextGeneration-first==n-1 && closes);
            observe(renderer);
            bool sameRects=previousRects.size()==fillRects.size();
            for (size_t i=0;sameRects && i<fillRects.size();++i)
                sameRects=previousRects[i].x==fillRects[i].x && previousRects[i].y==fillRects[i].y
                    && previousRects[i].w==fillRects[i].w && previousRects[i].h==fillRects[i].h;
            expect(prefix+"_phase_band_help_geometry_rollback",sameRects && previousLines==drawLines);
            expect(prefix+"_old_roles_sizes_colors",usesRoles(old) && color(textUses[1].color,20,21,22)
                   && records.at(old[1]).size==110 && drawColors.size()>5
                   && color(drawColors[1],0,0,0) && color(drawColors[4],73,164,121));
            expect(prefix+"_cursor_window_unchanged",cursorWrites==cursors && fullscreenCalls==fc
                   && SDL_ShowCursor(SDL_QUERY)==SDL_DISABLE && windows.count(window) && renderers.count(backend)
                   && wc==windowCreates && rc==rendererCreates);
            int afterW=0,afterH=0; SDL_GetWindowSize(window,&afterW,&afterH);
            expect(prefix+"_live_dimensions_preserved",afterW==windowW && afterH==windowH);
            SDL_Delay(1200); observe(renderer);
            expect(prefix+"_old_help_duration_preserved",renderer.isHelpOverlayVisible() && usesRoles(old));
            key(renderer,SDLK_p); observe(renderer);
            expect(prefix+"_old_preset_list_preserved",color(textUses[1].color,30,31,32));
            key(renderer,SDLK_p); observe(renderer);
            expect(prefix+"_base_snapshot_preserved",color(textUses[1].color,84,89,166));
            // Four opens succeed before any old close; callback renders at EACH old close.
            key(renderer,SDLK_F1); // Re-arm help before the candidate's shorter duration commits.
            actions.clear(); attempts=0; probeOld=oldLive;closeProbe=&renderer;
            probeNew={nextGeneration+1,nextGeneration+2,nextGeneration+3,nextGeneration+4};
            const auto recovered=reload(renderer); closeProbe=nullptr;probeOld.clear();
            const auto fresh=recentFour();
            expect(prefix+"_recovery_all_paths_sizes",exactSet(fresh,{fonts[3],fonts[2],fonts[1],fonts[0]},{31,81,19,17})
                   && attempts==4 && recovered.find("Config reloaded")!=std::string::npos);
            int opened=0; bool ordered=true;
            for (const auto& a:actions) { if (a.kind=="open") ++opened; if (a.kind=="close" && oldLive.count(a.generation)) ordered=ordered && opened==4; }
            expect(prefix+"_four_opens_before_old_closes",ordered && opened==4);
            observe(renderer);
            expect(prefix+"_recovery_initial_preset_and_cursor",usesRoles(fresh) && color(textUses[1].color,70,71,72)
                   && SDL_ShowCursor(SDL_QUERY)==SDL_ENABLE && cursorWrites==cursors+1);
            key(renderer,SDLK_p); observe(renderer);
            expect(prefix+"_fresh_preset_order",color(textUses[1].color,60,61,62));
            const auto repeat=reload(renderer); observe(renderer);
            expect(prefix+"_repeated_original_source_R",repeat.find("Config reloaded")!=std::string::npos
                   && color(textUses[1].color,70,71,72) && wc==windowCreates && rc==rendererCreates
                   && windows.count(window) && renderers.count(backend) && fullscreenCalls==fc);
            key(renderer,SDLK_f);
            expect("F_event_returns_windowed",!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)
                   && SDL_ShowCursor(SDL_QUERY)==SDL_ENABLE);
            expect("Q_event_quits",key(renderer,SDLK_q));
            expect("Esc_event_quits",key(renderer,SDLK_ESCAPE));
        }
        expect("R_cycles_final_no_owned_resources_"+std::to_string(n),liveFonts.empty() && windows.empty()
               && renderers.empty() && !sdlBalance && !ttfBalance);
    }
    write(roleConfig());
    {
        SdlRenderer renderer(parse({"--font",fonts[2]}));
        for (int cycle=0;cycle<3;++cycle) {
            write(roleConfig(cycle%2==0));
            reload(renderer);
            auto ids=recentFour(); bool global=true;
            for (int id:ids) global=global && records.at(id).path==fonts[2];
            expect("global_CLI_replayed_native_R_"+std::to_string(cycle),global);
        }
        // A real invalid font on R also rolls back (not just injected failures).
        write("font_path="+fonts[0]+"\nhelp_font_path="+(fixtureRoot/"missing.ttf").string()+"\n");
        const auto output=reload(renderer);
        expect("global_CLI_masks_nonempty_bad_role",output.find("Config reloaded")!=std::string::npos);
        write("help_font_path= \t \n");
        const auto before=liveIds();const int cursors=cursorWrites;attempts=0;
        const auto rejected=reload(renderer);
        expect("parser_failure_R_preserves_fonts_cursor",liveIds()==before && cursorWrites==cursors
               && attempts==0 && rejected.find("Config reloaded")==std::string::npos);
    }
    write(roleConfig());
    {
        SdlRenderer renderer(parse());
        key(renderer,SDLK_F1); const auto old=recentFour();
        const auto nonfont=fixtureRoot/"bad-R.txt";
        { std::ofstream out(nonfont); out << "not a font\n"; out.close(); must(static_cast<bool>(out),"Nonfont fixture write failed"); }
        for (const auto& bad:std::vector<std::string>{fixtureRoot.string(),nonfont.string()}) {
            write("font_path="+fonts[0]+"\nhelp_font_path="+bad+"\n");
            const auto failed=reload(renderer);observe(renderer);
            expect("actual_directory_nonfont_R_rollback_"+bad,usesRoles(old)
                   && occurrences(failed,"font open unsuccessful")==1
                   && failed.find("Config reloaded")==std::string::npos);
        }
        write("font_path="+fonts[0]+"\nhelp_font_path="+(fixtureRoot/"missing.ttf").string()+"\n");
        auto output=reload(renderer);observe(renderer);
        expect("actual_missing_role_R_rollback",usesRoles(old) && occurrences(output,"font open unsuccessful")==1
               && output.find("Config reloaded")==std::string::npos);
        write(roleConfig());output=reload(renderer);
        expect("actual_missing_role_R_recovery",output.find("Config reloaded")!=std::string::npos);
        SDL_Event event{};event.type=SDL_QUIT;
        must(SDL_PushEvent(&event)==1,"SDL_QUIT enqueue failed");
        expect("real_SDL_QUIT_event",renderer.pollQuit());
    }
}
}
int main() {
    try {
        Fixtures fixtures;
        for (const auto& f:fonts) must(fs::is_regular_file(f),"Required existing native font missing: "+f);
        startupFailures(); inheritanceAndPaths(); reloadBoundaries();
        bool exact=true;
        for (const auto& item:records) exact=exact && item.second.closes==1;
        expect("all_real_generations_closed_exactly_once",exact);
        expect("final_SDL_TTF_window_renderer_quiescence",liveFonts.empty() && windows.empty() && renderers.empty()
               && !sdlBalance && !ttfBalance && !SDL_WasInit(0) && !TTF_WasInit());
    } catch (const std::exception& e) {
        closeProbe=nullptr;
        std::cerr << "Native harness exception: " << e.what() << '\n';
        ++failures;
        cleanBaselineLeaks();
    }
    std::cout << "Native checks=" << checks << " real_font_generations=" << nextGeneration
              << " failures=" << failures << '\n';
    if (failures) return 1;
    std::cout << "All font transaction tests passed.\n";
    return 0;
}
