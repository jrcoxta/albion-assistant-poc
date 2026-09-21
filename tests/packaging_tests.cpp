#include <windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../resources/resource.h"

namespace {
void require(bool ok, const char* message) { if(!ok)throw std::runtime_error(message); }
}
int wmain(int argc, wchar_t** argv) {
    HMODULE module=nullptr;
    try {
        require(argc==2,"informe o executável do aplicativo");
        const std::filesystem::path executable=std::filesystem::absolute(argv[1]);
        DWORD binaryType=0;
        require(GetBinaryTypeW(executable.c_str(),&binaryType)&&binaryType==SCS_64BIT_BINARY,"aplicativo não é Windows x64");
        module=LoadLibraryExW(executable.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        require(module!=nullptr,"não foi possível ler os recursos do executável");
        const auto resource=[&](int id,LPCWSTR type){
            const auto found=FindResourceW(module,MAKEINTRESOURCEW(id),type);
            require(found!=nullptr,"recurso obrigatório não está embutido no executável");
            const auto size=SizeofResource(module,found);
            const auto data=LockResource(LoadResource(module,found));
            require(size>0&&data,"recurso vazio ou inválido");
            return std::string(static_cast<const char*>(data),size);
        };
        for(int id:{IDR_ASSASSIN_NONE,IDR_ASSASSIN_2,IDR_ASSASSIN_3,IDR_ASSASSIN_CLOCK,IDR_ASSASSIN_2_40,IDR_ASSASSIN_3_40,IDR_ASSASSIN_CLOCK_40}){
            const auto png=resource(id,RT_RCDATA);
            require(png.starts_with(std::string("\x89PNG\r\n\x1a\n",8)),"referência embutida não é PNG");
        }
        resource(IDI_APP,RT_GROUP_ICON);
        const auto manifest=resource(1,RT_MANIFEST);
        for(const char* expected:{"asInvoker","PerMonitorV2","Microsoft.Windows.Common-Controls"})
            require(manifest.find(expected)!=std::string::npos,"manifesto Windows incompleto");
        DWORD ignored=0;
        const DWORD size=GetFileVersionInfoSizeW(executable.c_str(),&ignored);
        require(size>0,"executável sem metadados de versão");
        std::vector<BYTE> version(size);
        require(GetFileVersionInfoW(executable.c_str(),0,size,version.data())!=FALSE,"metadados ilegíveis");
        wchar_t* product=nullptr;UINT length=0;
        require(VerQueryValueW(version.data(),L"\\StringFileInfo\\041604b0\\ProductName",reinterpret_cast<void**>(&product),&length)!=FALSE&&length>0&&product&&std::wstring(product)==L"Albion Assistant","nome de produto incorreto");
        FreeLibrary(module);
        std::cout<<"Executável x64 com ícone, manifesto, versão e referências embutidas verificados\n";
        return 0;
    }catch(const std::exception& e){if(module)FreeLibrary(module);std::cerr<<e.what()<<'\n';return 1;}
}
