#include "Script.h"

vector<Script*> Script::scripts;

Script::Script(char* path)
{
    FILE* file = fopen(path, "rb");

    if (file == NULL)
        throw VaultException("Script not found: %s", path);

    fclose(file);

    if (strstr(path, ".dll") || strstr(path, ".so"))
    {
        void* handle = NULL;
#ifdef __WIN32__
        handle = LoadLibrary(path);
#else
        handle = dlopen(path, RTLD_LAZY);
#endif
        if (handle == NULL)
            throw VaultException("Was not able to load C++ script: %s", path);

        this->handle = handle;
        this->cpp_script = true;
        scripts.push_back(this);

        GetScriptCallback(fexec, "exec", exec);
        GetScriptCallback(fOnClientAuthenticate, "OnClientAuthenticate", OnClientAuthenticate);
        GetScriptCallback(fOnPlayerConnect, "OnPlayerConnect", OnPlayerConnect);
        GetScriptCallback(fOnPlayerDisconnect, "OnPlayerDisconnect", OnPlayerDisconnect);
        GetScriptCallback(fOnPlayerRequestGame, "OnPlayerRequestGame", OnPlayerRequestGame);
        GetScriptCallback(fOnPlayerSpawn, "OnPlayerSpawn", OnPlayerSpawn);
        GetScriptCallback(fOnPlayerDeath, "OnPlayerDeath", OnPlayerDeath);
        GetScriptCallback(fOnPlayerCellChange, "OnPlayerCellChange", OnPlayerCellChange);

        SetScriptFunction("timestamp", &Utils::timestamp);
        SetScriptFunction("SetServerName", &Dedicated::SetServerName);
        SetScriptFunction("SetServerMap", &Dedicated::SetServerMap);
        SetScriptFunction("SetServerRule", &Dedicated::SetServerRule);
        SetScriptFunction("GetGameCode", &Dedicated::GetGameCode);

        exec();
    }
    else if (strstr(path, ".amx"))
    {
        AMX* vaultscript = new AMX();

        this->handle = (void*) vaultscript;
        this->cpp_script = false;
        scripts.push_back(this);

        cell ret = 0;
        int err = 0;

        err = PAWN::LoadProgram(vaultscript, path, NULL);
        if (err != AMX_ERR_NONE)
            throw VaultException("PAWN script %s error (%d): \"%s\"", path, err, aux_StrError(err));

        PAWN::CoreInit(vaultscript);
        PAWN::ConsoleInit(vaultscript);
        PAWN::FloatInit(vaultscript);
        PAWN::StringInit(vaultscript);
        PAWN::FileInit(vaultscript);
        PAWN::TimeInit(vaultscript);

        err = PAWN::RegisterVaultmpFunctions(vaultscript);
        if (err != AMX_ERR_NONE)
            throw VaultException("PAWN script %s error (%d): \"%s\"", path, err, aux_StrError(err));

        err = PAWN::Exec(vaultscript, &ret, AMX_EXEC_MAIN);
        if (err != AMX_ERR_NONE)
            throw VaultException("PAWN script %s error (%d): \"%s\"", path, err, aux_StrError(err));
    }
    else
        throw VaultException("Script type not recognized: %s", path);
}

Script::~Script()
{
    if (this->cpp_script)
    {
#ifdef __WIN32__
        FreeLibrary((HINSTANCE) this->handle);
#else
        dlclose(this->handle);
#endif
    }
    else
    {
        AMX* vaultscript = (AMX*) this->handle;
        PAWN::FreeProgram(vaultscript);
        delete vaultscript;
    }
}

void Script::LoadScripts(char* scripts)
{
    char* token = strtok(scripts, ",");

    try
    {
        while (token != NULL)
        {
            Script* script = new Script(token);
            token = strtok(NULL, ",");
        }
    }
    catch (...)
    {
        UnloadScripts();
        throw;
    }
}

void Script::UnloadScripts()
{
    vector<Script*>::iterator it;

    for (it = scripts.begin(); it != scripts.end(); ++it)
        delete *it;

    scripts.clear();
}

bool Script::Authenticate(int client, string name, string pwd)
{
    vector<Script*>::iterator it;
    bool result;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
            result = (*it)->OnClientAuthenticate(client, name, pwd);
        else
            result = (bool) PAWN::Call((AMX*) (*it)->handle, "OnClientAuthenticate", "ssi", 0, pwd.c_str(), name.c_str(), client);
    }

    return result;
}

void Script::Disconnect(int player, unsigned char reason)
{
    vector<Script*>::iterator it;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
            (*it)->OnPlayerDisconnect(player, reason);
        else
            PAWN::Call((AMX*) (*it)->handle, "OnPlayerDisconnect", "ii", 0, (int) reason, player);
    }
}

