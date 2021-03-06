#define _CRT_SECURE_NO_WARNINGS
#define LUA_LIB

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) \
 || defined(__TOS_WIN__) || defined(__WINDOWS__)
/* Compiling for Windows */
#ifndef __WINDOWS__
#define __WINDOWS__
#endif
#  include <Windows.h>
#endif/* Predefined Windows macros */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef __WINDOWS__
#include <unistd.h>
#include <dirent.h>
#define f_mkdir mkdir
#else
#include <direct.h>
#define f_mkdir(a, b) _mkdir(a)
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "luaIncludes.h"

#include "rikoFs.h"
#include <errno.h>

#include "rikoConsts.h"

#include <SDL2/SDL.h>

#ifdef __WINDOWS__
#define getFullPath(a, b) GetFullPathName(a, MAX_PATH, b, NULL)
#define rmdir(a) _rmdir(a)
#else
#define getFullPath(a, b) realpath(a, b)
#endif

#define checkPath(luaInput, varName)                                                                                      \
    do {                                                                                                                  \
        char* workingFront = (luaInput[0] == '\\' || luaInput[0] == '/') ? scriptsPath : currentWorkingDirectory;         \
                                                                                                                          \
        int scriptsPathLen = (int)strlen(scriptsPath);                                                                    \
                                                                                                                          \
        int ln = (int)(strlen(luaInput) + strlen(workingFront) + 2);                                                      \
        char *concatStr = (char*)malloc(ln);                                                                              \
        sprintf(concatStr, "%s/%s", workingFront, luaInput);                                                              \
                                                                                                                          \
        char* ptrz = (char*)getFullPath(concatStr, varName);                                                              \
        free(concatStr);                                                                                                  \
        if (ptrz == NULL && errno == ENOENT) {                                                                            \
            sprintf(varName, "%s/thisisatotallyrealpathyeponehundredpercent", scriptsPath);                               \
        }                                                                                                                 \
                                                                                                                          \
        int varNameLen = (int)strlen(varName);                                                                            \
                                                                                                                          \
        if (varNameLen + 1 == scriptsPathLen) {                                                                           \
            char tmp = scriptsPath[scriptsPathLen - 1];                                                                   \
            scriptsPath[scriptsPathLen - 1] = 0;                                                                          \
            if (strcmp(varName, scriptsPath) == 0) {                                                                      \
                scriptsPath[scriptsPathLen - 1] = tmp;                                                                    \
            } else {                                                                                                      \
                scriptsPath[scriptsPathLen - 1] = tmp;                                                                    \
                return luaL_error(L, "attempt to access file outside fs sandbox");                                        \
            }                                                                                                             \
        } else if (varNameLen > scriptsPathLen) {                                                                         \
            char tmp = varName[scriptsPathLen];                                                                           \
            varName[scriptsPathLen] = 0;                                                                                  \
            if (strcmp(varName, scriptsPath) == 0) {                                                                      \
                varName[scriptsPathLen] = tmp;                                                                            \
            } else {                                                                                                      \
                return luaL_error(L, "attempt to access file outside fs sandbox");                                        \
            }                                                                                                             \
        } else if (varNameLen == scriptsPathLen) {                                                                        \
            if (strcmp(varName, scriptsPath) != 0) {                                                                      \
                return luaL_error(L, "attempt to access file outside fs sandbox");                                        \
            }                                                                                                             \
        } else {                                                                                                          \
            return luaL_error(L, "attempt to access file outside fs sandbox");                                            \
        }                                                                                                                 \
    } while (0);

extern char* scriptsPath;
char currentWorkingDirectory[MAX_PATH];
char lastOpenedPath[MAX_PATH];

typedef struct {
    FILE *fileStream;
    bool open;
    bool canWrite;
    bool eof;
} fileHandleType;

static fileHandleType *checkFsObj(lua_State *L) {
    void *ud = luaL_checkudata(L, 1, "Riko4.fsObj");
    luaL_argcheck(L, ud != NULL, 1, "`FileHandle` expected");
    return (fileHandleType *)ud;
}

static int fsGetAttr(lua_State *L) {
    char filePath[MAX_PATH + 1];
    checkPath(luaL_checkstring(L, 1), filePath);

#ifdef __WINDOWS__
    unsigned long attr = GetFileAttributes(filePath);

    if (attr == INVALID_FILE_ATTRIBUTES) {
        lua_pushinteger(L, 0b11111111);
        
        return 1;
    }

    int attrOut = ((attr & FILE_ATTRIBUTE_READONLY)  != 0) * 0b00000001 +
                  ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) * 0b00000010;

    lua_pushinteger(L, attrOut);

    return 1;
#else
    struct stat statbuf;
    if (stat(filePath, &statbuf) != 0) {
        lua_pushinteger(L, 0b11111111);
        return 1;
    }

    int attrOut = ((access(filePath, W_OK)  * 0b00000001) +
                   (S_ISDIR(statbuf.st_mode) * 0b00000010));
    
    lua_pushinteger(L, attrOut);
    return 1;
#endif
}

static int fsList(lua_State *L) {
    char filePath[MAX_PATH + 1];
    checkPath(luaL_checkstring(L, 1), filePath);

#ifdef __WINDOWS__
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    char sPath[2048];

    //Specify a file mask. *.* = We want everything!
    sprintf(sPath, "%s\\*.*", filePath);

    if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE) {
        return 0;
    }

    lua_newtable(L);
    int i = 1;

    lua_pushinteger(L, i);
    lua_pushstring(L, ".");
    lua_rawset(L, -3);
    i++;

    lua_pushinteger(L, i);
    lua_pushstring(L, "..");
    lua_rawset(L, -3);
    i++;

    do {
        //Build up our file path using the passed in

        if (strcmp(fdFile.cFileName, ".") != 0
            && strcmp(fdFile.cFileName, "..") != 0) {
            lua_pushinteger(L, i);
            lua_pushstring(L, fdFile.cFileName);
            lua_rawset(L, -3);
            i++;
        }
    }
    while (FindNextFile(hFind, &fdFile)); //Find the next file.

    FindClose(hFind); //Always, Always, clean things up!

    return 1;
#else
    DIR *dp;
    struct dirent *ep;

    dp = opendir(filePath);
    if (dp != NULL) {
        lua_newtable(L);
        int i = 1;

        while (ep = readdir(dp)) {
            lua_pushinteger(L, i);
            lua_pushstring(L, ep->d_name);
            lua_rawset(L, -3);
            i++;
        }
        closedir(dp);

        return 1;
    } else {
        return 0;
    }
#endif
}

static int fsOpenFile(lua_State *L) {
    char filePath[MAX_PATH + 1];
    checkPath(luaL_checkstring(L, 1), filePath);

    const char *mode = luaL_checkstring(L, 2);

    FILE *fileHandle_o;
    if (mode[0] == 'w' || mode[0] == 'r' || mode[0] == 'a') {
        if (mode[1] == '+') {
            if (mode[2] == 'b' || mode[2] == 0) {
                fileHandle_o = fopen(filePath, mode);
            } else {
                return luaL_error(L, "invalid file mode");
            }
        } else if (mode[1] == 'b' && strlen(mode) == 2) {
            fileHandle_o = fopen(filePath, mode);
        } else if (strlen(mode) == 1) {
            fileHandle_o = fopen(filePath, mode);
        } else {
            return luaL_error(L, "invalid file mode");
        }
    } else {
        return luaL_error(L, "invalid file mode");
    }

    if (fileHandle_o == NULL)
        return 0;

    strcpy((char *) &lastOpenedPath, filePath + strlen(scriptsPath));
    for (int i = 0; lastOpenedPath[i] != '\0'; i++) {
        if (lastOpenedPath[i] == '\\')
            lastOpenedPath[i] = '/';
    }

    size_t nbytes = sizeof(fileHandleType);
    fileHandleType *outObj = (fileHandleType *)lua_newuserdata(L, nbytes);

    luaL_getmetatable(L, "Riko4.fsObj");
    lua_setmetatable(L, -2);

    /**outObj = {
        fileHandle_o,
        true,                 Emscripten doesn't like these
        mode[0] != 'r',
        false
    };*/

    outObj->fileStream = fileHandle_o;
    outObj->open = true;
    outObj->canWrite = mode[0] != 'r';
    outObj->eof = false;

    return 1;
}

static int fsLastOpened(lua_State *L) {
    lua_pushstring(L, (char*) &lastOpenedPath);

    return 1;
}

static int fsObjWrite(lua_State *L) {
    fileHandleType *data = checkFsObj(L);

    if (!data->open)
        return luaL_error(L, "file handle was closed");

    if (!data->canWrite)
        return luaL_error(L, "file is not open for writing");

    size_t strSize;
    const char *toWrite = luaL_checklstring(L, 2, &strSize);
    
    size_t written = fwrite(toWrite, 1, strSize, data->fileStream);

    lua_pushboolean(L, written == strSize);

    return 1;
}

int lineStrlen(char* s, int max) {
    for (int i = 0; i < max; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            return i + 1;
        }
    }

    return max;
}

static int fsObjRead(lua_State *L) {
    fileHandleType *data = checkFsObj(L);

    if (!data->open)
        return luaL_error(L, "file handle was closed");

    if (data->canWrite)
        return luaL_error(L, "file is open for writing");
    
    if (data->eof) {
        lua_pushnil(L);
        return 1;
    }

    if (feof(data->fileStream)) {
        data->eof = true;
        lua_pushnil(L);
        return 1;
    }

    int num = lua_gettop(L);
    if (num == 0) {
        // Read line

        size_t bufLen = sizeof(char) * (FS_LINE_INCR + 1);
        char *dataBuf = (char*)malloc(bufLen);
        if (dataBuf == NULL) return luaL_error(L, "unable to allocate enough memory for read operation");

        int st = ftell(data->fileStream);

        if (fgets(dataBuf, FS_LINE_INCR, data->fileStream) == NULL) {
            lua_pushstring(L, "");
            free(dataBuf);
            return 1;
        }

        size_t dataBufLen = lineStrlen(dataBuf, FS_LINE_INCR);
        if (feof(data->fileStream)) {
            data->eof = true;
            dataBufLen = ftell(data->fileStream);
            lua_pushlstring(L, dataBuf, dataBufLen - st);
            free(dataBuf);
            return 1;
        } else if (dataBuf[dataBufLen - 1] == '\n' || dataBuf[dataBufLen - 1] == '\r') {
            lua_pushlstring(L, dataBuf, dataBufLen - 1);
            free(dataBuf);
            return 1;
        }

        st = ftell(data->fileStream);

        for (int i = 1;; i++) {
            bufLen += sizeof(char) * FS_LINE_INCR;
            dataBuf = (char*)realloc(dataBuf, bufLen);
            if (dataBuf == NULL) return luaL_error(L, "unable to allocate enough memory for read operation");

            st = ftell(data->fileStream);

            if (fgets(dataBuf + i * (FS_LINE_INCR - 1) * sizeof(char), FS_LINE_INCR, data->fileStream) == NULL) {
                lua_pushstring(L, "");
                free(dataBuf);
                return 1;
            }

            size_t dataBufLen = lineStrlen(dataBuf, bufLen / sizeof(char));
            if (feof(data->fileStream)) {
                data->eof = true;
                dataBufLen = ftell(data->fileStream);
                lua_pushlstring(L, dataBuf, dataBufLen - st);
                free(dataBuf);
                return 1;
            } else if (dataBuf[dataBufLen - 1] == '\n') {
                lua_pushlstring(L, dataBuf, dataBufLen - 1);
                free(dataBuf);
                return 1;
            }
        }

        return 0;
    }

    int type = lua_type(L, 1 - num);
    if (type == LUA_TSTRING) {
        const char *mode = luaL_checkstring(L, 2);

        if (mode[0] == '*') {
            if (mode[1] == 'a') {
                // All

                fseek(data->fileStream, 0, SEEK_END);
                long lSize = ftell(data->fileStream);
                rewind(data->fileStream);

                char *dataBuf = (char*)malloc(sizeof(char) * (lSize + 1));
                if (dataBuf == NULL) return luaL_error(L, "unable to allocate enough memory for read operation");

                size_t result = fread(dataBuf, 1, lSize, data->fileStream);

                dataBuf[result] = 0;

                lua_pushlstring(L, dataBuf, result);

                data->eof = true;

                free(dataBuf);

                return 1;
            } else if (mode[1] == 'l') {
                // Read line

                size_t bufLen = sizeof(char) * (FS_LINE_INCR + 1);
                char *dataBuf = (char*)malloc(bufLen);
                if (dataBuf == NULL) return luaL_error(L, "unable to allocate enough memory for read operation");

                int st = ftell(data->fileStream);

                if (fgets(dataBuf, FS_LINE_INCR, data->fileStream) == NULL) {
                    lua_pushstring(L, "");
                    free(dataBuf);
                    return 1;
                }

                size_t dataBufLen = lineStrlen(dataBuf, FS_LINE_INCR);
                if (feof(data->fileStream)) {
                    data->eof = true;
                    dataBufLen = ftell(data->fileStream);
                    lua_pushlstring(L, dataBuf, dataBufLen - st);
                    free(dataBuf);
                    return 1;
                } else if (dataBuf[dataBufLen - 1] == '\n' || dataBuf[dataBufLen - 1] == '\r') {
                    lua_pushlstring(L, dataBuf, dataBufLen - 1);
                    free(dataBuf);
                    return 1;
                }

                st = ftell(data->fileStream);

                for (int i = 1;; i++) {
                    bufLen += sizeof(char) * FS_LINE_INCR;
                    dataBuf = (char*)realloc(dataBuf, bufLen);
                    if (dataBuf == NULL) return luaL_error(L, "unable to allocate enough memory for read operation");
                    
                    st = ftell(data->fileStream);

                    if (fgets(dataBuf + i * (FS_LINE_INCR - 1) * sizeof(char), FS_LINE_INCR, data->fileStream) == NULL) {
                        lua_pushstring(L, "");
                        free(dataBuf);
                        return 1;
                    }

                    size_t dataBufLen = lineStrlen(dataBuf, bufLen / sizeof(char));
                    if (feof(data->fileStream)) {
                        data->eof = true;
                        dataBufLen = ftell(data->fileStream);
                        lua_pushlstring(L, dataBuf, dataBufLen - st);
                        free(dataBuf);
                        return 1;
                    } else if (dataBuf[dataBufLen - 1] == '\n' || dataBuf[dataBufLen - 1] == '\r') {
                        lua_pushlstring(L, dataBuf, dataBufLen - 1);
                        free(dataBuf);
                        return 1;
                    }
                }
            } else {
                return luaL_argerror(L, 2, "invalid mode");
            }
        } else {
            return luaL_argerror(L, 2, "invalid mode");
        }
    } else if (type == LUA_TNUMBER) {
        int len = luaL_checkinteger(L, 2);

        // Read 'len' bytes

        char *dataBuf = (char*)malloc(sizeof(char) * (len + 1));
        if (dataBuf == NULL) return luaL_error(L, "unable to allocate enough memory for read operation");

        size_t result = fread(dataBuf, sizeof(char), len, data->fileStream);

        dataBuf[result] = 0;

        lua_pushlstring(L, dataBuf, result);

        free(dataBuf);

        return 1;
    } else {
        const char *typen = lua_typename(L, type);
        int len = 16 + strlen(typen);
        char *emsg = (char*)malloc(len);
        sprintf(emsg, "%s was unexpected", typen);
        return luaL_argerror(L, 2, emsg);
        free(emsg);
    }

    return 0;
}

static int fsMkDir(lua_State *L) {
    char filePath[MAX_PATH + 1];
    checkPath(luaL_checkstring(L, 1), filePath);

    lua_pushboolean(L, f_mkdir(filePath, 0777) + 1);
    return 1;
}

static int fsMv(lua_State *L) {
    char filePath[MAX_PATH + 1];
    checkPath(luaL_checkstring(L, 1), filePath);

    char endPath[MAX_PATH + 1];
    checkPath(luaL_checkstring(L, 2), endPath);

    lua_pushboolean(L, rename(filePath, endPath) + 1);
    return 1;
}

static int fsDelete(lua_State *L) {
    char filePath[MAX_PATH + 1];
    checkPath(luaL_checkstring(L, 1), filePath);
    
    if (remove(filePath) == 0)
        lua_pushboolean(L, true);
    else
        lua_pushboolean(L, rmdir(filePath) + 1);

    return 1;
}

static int fsObjCloseHandle(lua_State *L) {
    fileHandleType *data = checkFsObj(L);

    if (data->open)
        fclose(data->fileStream);

    data->open = false;

    return 0;
}

static int fsGetCWD(lua_State *L) {
    lua_pushstring(L, currentWorkingDirectory + strlen(scriptsPath));

    return 1;
}

static int fsSetCWD(lua_State *L) {
    const char* nwd = luaL_checkstring(L, 1);

    if (nwd[0] == '\\' || nwd[0] == '/') {
        int nwdlen = strlen(nwd);

        int ln = (int)(strlen(scriptsPath) + nwdlen + 2);
        char *concatStr = (char*)malloc(ln);
        sprintf(concatStr, "%s/%s", scriptsPath, nwd);

        char *fpath = (char*)malloc(sizeof(char) * MAX_PATH);

        getFullPath(concatStr, fpath);

        free(concatStr);

        if (strlen(fpath) < MAX_PATH) {
            strncpy(currentWorkingDirectory, fpath, strlen(fpath));
            currentWorkingDirectory[strlen(fpath)] = 0;
        }

        free(fpath);
    } else {
        int nwdlen = strlen(nwd);

        int ln = (int)(strlen(currentWorkingDirectory) + nwdlen + 2);
        char *concatStr = (char*) malloc(ln);
        sprintf(concatStr, "%s/%s", currentWorkingDirectory, nwd);

        char *fpath = (char*) malloc(sizeof(char) * MAX_PATH);

        getFullPath(concatStr, fpath);

        free(concatStr);

        if (strlen(fpath) < MAX_PATH) {
            strncpy(currentWorkingDirectory, fpath, strlen(fpath));
            currentWorkingDirectory[strlen(fpath)] = 0;
        }

        free(fpath);
    }

    return 0;
}

static int fsGetClipboardText(lua_State *L) {
    if (SDL_HasClipboardText()) {
        char *text = SDL_GetClipboardText();

        lua_pushstring(L, text);

        SDL_free(text);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int fsSetClipboardText(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);

    SDL_SetClipboardText(text);

    return 0;
}

static const luaL_Reg fsLib[] = {
    { "getAttr", fsGetAttr },
    { "open", fsOpenFile },
    { "list", fsList },
    { "delete", fsDelete },
    { "mkdir", fsMkDir },
    { "move", fsMv },
    { "setCWD", fsSetCWD },
    { "getCWD", fsGetCWD },
    { "getLastFile", fsLastOpened },
    { "getClipboard", fsGetClipboardText },
    { "setClipboard", fsSetClipboardText },
    { NULL, NULL }
};

static const luaL_Reg fsLib_m[] = {
    { "read", fsObjRead },
    { "write", fsObjWrite },
    { "close", fsObjCloseHandle },
    { NULL, NULL }
};

LUALIB_API int luaopen_fs(lua_State *L) {
    char *fpath = (char*)malloc(sizeof(char) * MAX_PATH);
    getFullPath(scriptsPath, fpath);

    if (strlen(fpath) < MAX_PATH)
        strncpy(currentWorkingDirectory, fpath, strlen(fpath));
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to initialize cwd");
        return 2;
    }
    
    free(fpath);

    luaL_newmetatable(L, "Riko4.fsObj");

    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);  /* pushes the metatable */
    lua_settable(L, -3);  /* metatable.__index = metatable */

    luaL_openlib(L, NULL, fsLib_m, 0);

    luaL_openlib(L, RIKO_FS_NAME, fsLib, 0);
    return 1;
}
