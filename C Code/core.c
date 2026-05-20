#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include "projeto.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* ========== CONSTANTES ========== */
#define PASTA_PRESENCAS  "presencas"
#define ARQUIVO_PADRAO   "registros.json"
#define RELATORIO_PADRAO "relatorio.xlsx"

char *ROTULO_ATIVO[] = { "Nao", "Sim", "Visitante" };
char *ROTULO_IDADE[] = { "", "Juvenil", "Adulto", "Jovem", "Crianca" };

/* ========== MACROS ========== */
#define CONVERTER_ATIVO(texto) (converterTextoParaAtivo(texto))
#define JSON_SUCESSO(mensagem)  printf("{\"status\": \"%s\"}\n", mensagem)

/* ========== ARQUIVOS ========== */
FILE *abrirArquivo(char *caminho, char *modo) {
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, 0, caminho, -1, NULL, 0);
    wchar_t *wc = malloc((n + 30) * sizeof(wchar_t));
    if (!wc) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, caminho, -1, wc, n);
    MultiByteToWideChar(CP_UTF8, 0, modo, -1, wc + n, 30);
    FILE *f = _wfopen(wc, wc + n);
    free(wc);
    return f;
#else
    return fopen(caminho, modo);
#endif
}

char *lerArquivo(char *caminho, long *tamanho) {
    FILE *arquivo = abrirArquivo(caminho, "rb");
    if (!arquivo) return NULL;

    fseek(arquivo, 0, SEEK_END);
    *tamanho = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);

    char *conteudo = malloc(*tamanho + 1);
    if (!conteudo) { fclose(arquivo); return NULL; }

    fread(conteudo, 1, *tamanho, arquivo);
    fclose(arquivo);
    conteudo[*tamanho] = '\0';
    return conteudo;
}

/* ========== JSON ========== */
void escreverTextoJson(FILE *arquivo, char *texto) {
    fputc('"', arquivo);
    for (; *texto; texto++) {
        switch (*texto) {
            case '\\': fputs("\\\\", arquivo); break;
            case '"':  fputs("\\\"", arquivo); break;
            case '\n': fputs("\\n", arquivo);  break;
            case '\r': fputs("\\r", arquivo);  break;
            case '\t': fputs("\\t", arquivo);  break;
            default: if ((char)*texto < 32) fprintf(arquivo, "\\u%04x", *texto); else fputc(*texto, arquivo);
        }
    }
    fputc('"', arquivo);
}

/* Localiza o fim do objeto atual */
char *encontrarFimObjetoJson(char *ponteiro) {
    int profundidade = 0;
    int dentro_de_texto = 0;
    int escape = 0;
    while (*ponteiro) {
        if (escape) escape = 0;
        else if (*ponteiro == '\\' && dentro_de_texto) escape = 1;
        else if (*ponteiro == '"') dentro_de_texto = !dentro_de_texto;
        else if (!dentro_de_texto) {
            if (*ponteiro == '{') profundidade++;
            else if (*ponteiro == '}' && --profundidade == 0) return ponteiro;
        }
        ponteiro++;
    }
    return NULL;
}

char *encontrarValorJson(char *json, char *chave) {
    if (!json) return NULL;
    char chave_entre_aspas[256];
    snprintf(chave_entre_aspas, sizeof chave_entre_aspas, "\"%s\"", chave);
    char *inicio_valor = strstr(json, chave_entre_aspas);
    if (!inicio_valor) return NULL;
    inicio_valor = strchr(inicio_valor, ':') + 1;
    while (*inicio_valor == ' ' || *inicio_valor == '\t') inicio_valor++;
    return inicio_valor;
}

int lerTextoJson(char *json, char *chave, char *destino, int tamanho_destino) {
    char *inicio_valor = encontrarValorJson(json, chave);
    if (!inicio_valor || *inicio_valor != '"') return 0;
    inicio_valor++;
    int indice = 0;
    while (indice < tamanho_destino - 1 && *inicio_valor && *inicio_valor != '"') {
        if (*inicio_valor == '\\') { inicio_valor++; if (!*inicio_valor) break; }
        destino[indice++] = *inicio_valor++;
    }
    destino[indice] = '\0';
    return *inicio_valor == '"';
}

int lerBooleanoJson(char *json, char *chave, int *destino) {
    char *inicio_valor = encontrarValorJson(json, chave);
    if (!inicio_valor || !destino) return 0;
    while (*inicio_valor == ' ' || *inicio_valor == '\t') inicio_valor++;

    if (strncmp(inicio_valor, "true", 4) == 0) {
        *destino = 1;
        return 1;
    }
    if (strncmp(inicio_valor, "false", 5) == 0) {
        *destino = 0;
        return 1;
    }
    if (*inicio_valor == '"') {
        char texto[16] = "";
        if (!lerTextoJson(json, chave, texto, sizeof texto)) return 0;
        *destino = strcmp(texto, "true") == 0 || strcmp(texto, "1") == 0 || strcmp(texto, "Sim") == 0;
        return 1;
    }
    return 0;
}

/* ========== CONVERSOES ========== */
FaixaIdade textoParaFaixaIdade(char *texto) {
    if (!texto) return IDADE_INDEFINIDO;
    for (int i = 1; i <= 4; i++) 
        if (strcmp(texto, ROTULO_IDADE[i]) == 0) return (FaixaIdade)i;
    return IDADE_INDEFINIDO;
}

AtivoMembro converterTextoParaAtivo(char *texto) {
    if (!texto) return ATIVO_NAO;
    if (strcmp(texto, "Sim") == 0) return ATIVO_SIM;
    if (strcmp(texto, "Visitante") == 0) return ATIVO_VISITANTE;
    return ATIVO_NAO;
}

/* ========== CADASTRO ========== */
int carregarBaseDados(BaseDados *base, char *caminho) {
    memset(base, 0, sizeof *base);
    
    long tamanho = 0;
    char *conteudo = lerArquivo(caminho, &tamanho);
    if (!conteudo) return 0;
    
    char *inicio_objeto = conteudo;
    while (*inicio_objeto && base->quantidade < MAX_REGISTROS) {
        inicio_objeto = strchr(inicio_objeto, '{');
        if (!inicio_objeto) break;
        
        char *fim_objeto = encontrarFimObjetoJson(inicio_objeto);
        if (!fim_objeto) break;
        
        int tamanho_objeto = (int)(fim_objeto - inicio_objeto + 1);
        char *objeto = malloc(tamanho_objeto + 1);
        if (!objeto) break;
        
        memcpy(objeto, inicio_objeto, tamanho_objeto);
        objeto[tamanho_objeto] = '\0';
        
        Registro *registro = &base->registros[base->quantidade];
        memset(registro, 0, sizeof *registro);
        if (!lerTextoJson(objeto, "nome", registro->nome, sizeof registro->nome)) {
            free(objeto);
            inicio_objeto = fim_objeto + 1;
            continue;
        }
        
        char texto_temporario[64] = "";
        if (!lerTextoJson(objeto, "identificador", texto_temporario, sizeof texto_temporario)) {
            free(objeto);
            inicio_objeto = fim_objeto + 1;
            continue;
        }

        registro->identificador = atoi(texto_temporario);
        if (registro->identificador <= 0) {
            free(objeto);
            inicio_objeto = fim_objeto + 1;
            continue;
        }

        if (lerTextoJson(objeto, "ativo", texto_temporario, sizeof texto_temporario))
            registro->ativo = CONVERTER_ATIVO(texto_temporario);
        if (lerTextoJson(objeto, "idade", texto_temporario, sizeof texto_temporario))
            registro->idade = textoParaFaixaIdade(texto_temporario);
        
        base->quantidade++;
        free(objeto);
        inicio_objeto = fim_objeto + 1;
    }
    free(conteudo);
    return base->quantidade;
}

int salvarBaseDados(BaseDados *base, char *caminho) {
    FILE *arquivo = abrirArquivo(caminho, "w");
    if (!arquivo) return 0;
    
    fputs("[\n", arquivo);
    for (int i = 0; i < base->quantidade; i++) {
        Registro *registro = &base->registros[i];
        char texto_identificador[32];
        snprintf(texto_identificador, sizeof texto_identificador, "%d", registro->identificador);
        fprintf(arquivo, "  {\"identificador\": ");
        escreverTextoJson(arquivo, texto_identificador);
        fprintf(arquivo, ",\"nome\": ");
        escreverTextoJson(arquivo, registro->nome);
        fprintf(arquivo, ",\"ativo\": ");
        escreverTextoJson(arquivo, ROTULO_ATIVO[registro->ativo]);
        fprintf(arquivo, ",\"idade\": ");
        escreverTextoJson(arquivo, ROTULO_IDADE[registro->idade]);
        fprintf(arquivo, "}%s\n", i < base->quantidade - 1 ? "," : "");
    }
    fputs("]\n", arquivo);
    fclose(arquivo);
    return 1;
}

Registro *buscarRegistro(BaseDados *base, char *nome) {
    if (!nome) return NULL;
    for (int indice = 0; indice < base->quantidade; indice++)
        if (strcmp(base->registros[indice].nome, nome) == 0) return &base->registros[indice];
    return NULL;
}

int proximoIdentificador(BaseDados *base) {
    int proximo = 1;
    for (int i = 0; i < base->quantidade; i++)
        if (base->registros[i].identificador >= proximo) proximo = base->registros[i].identificador + 1;
    return proximo;
}


int adicionarRegistro(BaseDados *base, char *nome, AtivoMembro ativo, FaixaIdade idade) {
    if (!nome || !*nome || strlen(nome) >= TAM_NOME) return 0;
    if (buscarRegistro(base, nome) || base->quantidade >= MAX_REGISTROS) return 0;

    int novo_identificador = proximoIdentificador(base);
    Registro *registro = &base->registros[base->quantidade++];
    memset(registro, 0, sizeof *registro);
    registro->identificador = novo_identificador;
    strncpy(registro->nome, nome, TAM_NOME - 1);
    registro->nome[TAM_NOME - 1] = '\0';
    registro->ativo = ativo;
    registro->idade = idade;
    return 1;
}

int editarRegistro(BaseDados *base, char *nomeAnterior, char *nomeNovo, AtivoMembro ativo, FaixaIdade idade) {
    Registro *registro = buscarRegistro(base, nomeAnterior);
    if (!registro) return 0;
    if (nomeNovo && *nomeNovo && strcmp(nomeNovo, nomeAnterior) != 0 && buscarRegistro(base, nomeNovo)) return 0;

    if (nomeNovo && *nomeNovo) {
        strncpy(registro->nome, nomeNovo, TAM_NOME - 1);
        registro->nome[TAM_NOME - 1] = '\0';
    }
    registro->ativo = ativo;
    registro->idade = idade;
    return 1;
}

int removerRegistro(BaseDados *base, char *nome) {
    for (int indice = 0; indice < base->quantidade; indice++) {
        if (strcmp(base->registros[indice].nome, nome) != 0) continue;
        memmove(&base->registros[indice], &base->registros[indice + 1], (base->quantidade - indice - 1) * sizeof(Registro));
        base->quantidade--;
        return 1;
    }
    return 0;
}

/* ========== SAIDA JSON ========== */
void imprimirRegistroJson(FILE *saida, Registro *registro) {
    fprintf(saida, "{\"identificador\": \"%d\",\"nome\": ", registro->identificador);
    escreverTextoJson(saida, registro->nome);
    fprintf(saida, ",\"ativo\": ");
    escreverTextoJson(saida, ROTULO_ATIVO[registro->ativo]);
    fprintf(saida, ",\"idade\": ");
    escreverTextoJson(saida, ROTULO_IDADE[registro->idade]);
    fprintf(saida, "}");
}

void imprimirRegistro(Registro *registro) {
    imprimirRegistroJson(stdout, registro);
    putchar('\n');
}

void imprimirLista(BaseDados *base) {
    fputs("[\n", stdout);
    for (int indice = 0; indice < base->quantidade; indice++) {
        fputs("  ", stdout);
        imprimirRegistroJson(stdout, &base->registros[indice]);
        if (indice < base->quantidade - 1) fputs(",\n", stdout);
    }
    fputs("]\n", stdout);
}

/* ========== PRESENÇA ========== */
void carregarPresencas(char *caminho, RegistroPresencaArquivo *registros, int maximo_registros, int *quantidade_saida) {
    *quantidade_saida = 0;
    long tamanho = 0;
    char *conteudo = lerArquivo(caminho, &tamanho);
    if (!conteudo) return;
    
    int quantidade = 0;
    for (char *inicio_objeto = conteudo; *inicio_objeto && quantidade < maximo_registros; ) {
        inicio_objeto = strchr(inicio_objeto, '{');
        if (!inicio_objeto) break;
        
        char *fim_objeto = encontrarFimObjetoJson(inicio_objeto);
        if (!fim_objeto) break;
        
        int tamanho_objeto = (int)(fim_objeto - inicio_objeto + 1);
        char *objeto = malloc(tamanho_objeto + 1);
        if (!objeto) break;
        
        memcpy(objeto, inicio_objeto, tamanho_objeto);
        objeto[tamanho_objeto] = '\0';
        
        registros[quantidade].nome[0] = '\0';
        registros[quantidade].presenca[0] = '\0';
        registros[quantidade].identificador = 0;
        registros[quantidade].marcado = 0;
        lerTextoJson(objeto, "nome", registros[quantidade].nome, sizeof registros[quantidade].nome);
        lerTextoJson(objeto, "presenca", registros[quantidade].presenca, sizeof registros[quantidade].presenca);
        if (!lerBooleanoJson(objeto, "marcado", &registros[quantidade].marcado))
            registros[quantidade].marcado = strcmp(registros[quantidade].presenca, "Sim") == 0;
        
        char texto_identificador[16] = "0";
        if (lerTextoJson(objeto, "identificador", texto_identificador, sizeof texto_identificador))
            registros[quantidade].identificador = atoi(texto_identificador);
        
        quantidade++;
        free(objeto);
        inicio_objeto = fim_objeto + 1;
    }
    *quantidade_saida = quantidade;
    free(conteudo);
}

void extrairData(char *nome, char *saida) {
    char *ponteiro = nome;
    while (*ponteiro) {
        if (isdigit(ponteiro[0]) && isdigit(ponteiro[1]) && isdigit(ponteiro[2]) && isdigit(ponteiro[3]) &&
            ponteiro[4] == '-' && isdigit(ponteiro[5]) && isdigit(ponteiro[6]) && 
            ponteiro[7] == '-' && isdigit(ponteiro[8]) && isdigit(ponteiro[9])) {
            memcpy(saida, ponteiro, 10);
            saida[10] = '\0';
            return;
        }
        ponteiro++;
    }
    
    char *nome_base = strrchr(nome, '\\');
    nome_base = nome_base ? nome_base + 1 : nome;
    char *extensao = strrchr(nome_base, '.');
    int tamanho_nome = extensao ? (int)(extensao - nome_base) : (int)strlen(nome_base);
    if (tamanho_nome > 15) tamanho_nome = 15;
    memcpy(saida, nome_base, tamanho_nome);
    saida[tamanho_nome] = '\0';
}

int compararTextos(const void *a, const void *b) {
    return strcmp(*(char**)a, *(char**)b);
}

#ifdef _WIN32
int listarArquivosJson(char *pasta, char caminhos[][TAM_CAMINHO], char nomes[][TAM_NOME], int maximo) {
    char padrao[TAM_CAMINHO];
    if (snprintf(padrao, sizeof padrao, "%s\\*.json", pasta) < 0) return 0;
    
    wchar_t padrao_wide[TAM_CAMINHO];
    MultiByteToWideChar(CP_UTF8, 0, padrao, -1, padrao_wide, TAM_CAMINHO);
    
    WIN32_FIND_DATAW dados_arquivo;
    HANDLE busca = FindFirstFileW(padrao_wide, &dados_arquivo);
    if (busca == INVALID_HANDLE_VALUE) return 0;
    
    int quantidade = 0;
    do {
        char nome_utf8[TAM_NOME];
        WideCharToMultiByte(CP_UTF8, 0, dados_arquivo.cFileName, -1, nome_utf8, TAM_NOME, NULL, NULL);
        if (nome_utf8[0] != '~' && quantidade < maximo) {
            snprintf(nomes[quantidade], TAM_NOME, "%s", nome_utf8);
            snprintf(caminhos[quantidade], TAM_CAMINHO, "%s\\%s", pasta, nome_utf8);
            quantidade++;
        }
    } while (FindNextFileW(busca, &dados_arquivo) && quantidade < maximo);
    FindClose(busca);
    return quantidade;
}
#else
int listarArquivosJson(char *pasta, char caminhos[][TAM_CAMINHO], char nomes[][TAM_NOME], int maximo) { 
    (void)pasta; (void)caminhos; (void)nomes; (void)maximo; return 0; 
}
#endif

int terminaCom(char *texto, char *sufixo) {
    if (!texto || !sufixo) return 0;
    size_t tamanho_texto = strlen(texto);
    size_t tamanho_sufixo = strlen(sufixo);
    return tamanho_texto >= tamanho_sufixo && strcmp(texto + tamanho_texto - tamanho_sufixo, sufixo) == 0;
}

int compararTextoSemCaso(char *a, char *b) {
    if (!a) a = "";
    if (!b) b = "";
    while (*a && *b) {
        unsigned char ca = (unsigned char)tolower((unsigned char)*a);
        unsigned char cb = (unsigned char)tolower((unsigned char)*b);
        if (ca != cb) return (int)ca - (int)cb;
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

void montarCaminhoChamada(char *saida, int tamanho_saida, char *pasta, char *chamada) {
    if (!saida || tamanho_saida <= 0) return;
    if (!pasta) pasta = PASTA_PRESENCAS;
    if (!chamada) chamada = "";
    snprintf(saida, tamanho_saida, "%s\\%s%s", pasta, chamada, terminaCom(chamada, ".json") ? "" : ".json");
}

int criarPastaSeNecessario(char *pasta) {
    if (!pasta || !*pasta) return 0;
#ifdef _WIN32
    wchar_t pasta_wide[TAM_CAMINHO];
    MultiByteToWideChar(CP_UTF8, 0, pasta, -1, pasta_wide, TAM_CAMINHO);
    if (CreateDirectoryW(pasta_wide, NULL)) return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return 1;
#endif
}

int arquivoExiste(char *caminho) {
    FILE *arquivo = abrirArquivo(caminho, "rb");
    if (!arquivo) return 0;
    fclose(arquivo);
    return 1;
}

void obterDataArquivo(char *caminho, char *saida, int tamanho_saida) {
    if (!saida || tamanho_saida <= 0) return;
    saida[0] = '\0';
#ifdef _WIN32
    if (!caminho) return;
    wchar_t caminho_wide[TAM_CAMINHO];
    MultiByteToWideChar(CP_UTF8, 0, caminho, -1, caminho_wide, TAM_CAMINHO);

    WIN32_FILE_ATTRIBUTE_DATA dados;
    if (!GetFileAttributesExW(caminho_wide, GetFileExInfoStandard, &dados)) return;

    FILETIME local;
    SYSTEMTIME sistema;
    if (!FileTimeToLocalFileTime(&dados.ftCreationTime, &local)) return;
    if (!FileTimeToSystemTime(&local, &sistema)) return;
    snprintf(saida, tamanho_saida, "%02d/%02d/%04d", sistema.wDay, sistema.wMonth, sistema.wYear);
#else
    (void)caminho;
#endif
}

RegistroPresencaArquivo *buscarPresencaArquivo(RegistroPresencaArquivo *presencas, int quantidade, Registro *registro) {
    if (!presencas || !registro) return NULL;
    for (int i = 0; i < quantidade; i++) {
        if (presencas[i].identificador > 0 && presencas[i].identificador == registro->identificador)
            return &presencas[i];
    }
    for (int i = 0; i < quantidade; i++) {
        if (presencas[i].nome[0] && strcmp(presencas[i].nome, registro->nome) == 0)
            return &presencas[i];
    }
    return NULL;
}

int presencaValida(char *presenca) {
    return presenca && (strcmp(presenca, "Sim") == 0 || strcmp(presenca, "Nao") == 0);
}

typedef struct {
    Registro *registro;
    char      presenca[16];
    int       marcado;
} LinhaPresenca;

int compararLinhasPresenca(const void *a, const void *b) {
    const LinhaPresenca *linha_a = (const LinhaPresenca*)a;
    const LinhaPresenca *linha_b = (const LinhaPresenca*)b;
    if (linha_a->marcado != linha_b->marcado) return linha_b->marcado - linha_a->marcado;
    return compararTextoSemCaso(linha_a->registro->nome, linha_b->registro->nome);
}

int montarLinhasPresenca(BaseDados *base, char *arquivo_presenca, LinhaPresenca *linhas, int maximo_linhas) {
    if (!base || !linhas) return 0;

    RegistroPresencaArquivo presencas[MAX_PASTAS];
    int total_presencas = 0;
    if (arquivo_presenca && arquivoExiste(arquivo_presenca))
        carregarPresencas(arquivo_presenca, presencas, MAX_PASTAS, &total_presencas);

    int quantidade = 0;
    for (int i = 0; i < base->quantidade && quantidade < maximo_linhas; i++) {
        Registro *registro = &base->registros[i];
        LinhaPresenca *linha = &linhas[quantidade++];
        memset(linha, 0, sizeof *linha);
        linha->registro = registro;

        RegistroPresencaArquivo *entrada = buscarPresencaArquivo(presencas, total_presencas, registro);
        if (entrada && presencaValida(entrada->presenca) && entrada->marcado) {
            strncpy(linha->presenca, entrada->presenca, sizeof linha->presenca - 1);
            linha->presenca[sizeof linha->presenca - 1] = '\0';
            linha->marcado = 1;
        }
    }

    qsort(linhas, quantidade, sizeof(LinhaPresenca), compararLinhasPresenca);
    return quantidade;
}

void imprimirLinhaPresencaJson(FILE *saida, LinhaPresenca *linha) {
    fprintf(saida, "{\"identificador\": \"%d\",\"nome\": ", linha->registro->identificador);
    escreverTextoJson(saida, linha->registro->nome);
    fprintf(saida, ",\"ativo\": ");
    escreverTextoJson(saida, ROTULO_ATIVO[linha->registro->ativo]);
    fprintf(saida, ",\"idade\": ");
    escreverTextoJson(saida, ROTULO_IDADE[linha->registro->idade]);
    fprintf(saida, ",\"presenca\": ");
    escreverTextoJson(saida, linha->marcado ? linha->presenca : "");
    fprintf(saida, ",\"marcado\": %s}", linha->marcado ? "true" : "false");
}

void imprimirLinhasPresenca(BaseDados *base, char *arquivo_presenca) {
    LinhaPresenca linhas[MAX_REGISTROS];
    int quantidade = montarLinhasPresenca(base, arquivo_presenca, linhas, MAX_REGISTROS);

    fputs("[\n", stdout);
    for (int i = 0; i < quantidade; i++) {
        fputs("  ", stdout);
        imprimirLinhaPresencaJson(stdout, &linhas[i]);
        if (i < quantidade - 1) fputs(",\n", stdout);
    }
    fputs("]\n", stdout);
}

int salvarChamadaPresenca(BaseDados *base, char *pasta, char *chamada, char *entrada_marcacoes, char *caminho_saida, int tamanho_saida) {
    if (!base || !chamada || !*chamada || !entrada_marcacoes) return 0;
    if (!criarPastaSeNecessario(pasta)) return 0;

    RegistroPresencaArquivo marcacoes[MAX_PASTAS];
    int total_marcacoes = 0;
    carregarPresencas(entrada_marcacoes, marcacoes, MAX_PASTAS, &total_marcacoes);

    char caminho[TAM_CAMINHO];
    montarCaminhoChamada(caminho, sizeof caminho, pasta, chamada);
    FILE *arquivo = abrirArquivo(caminho, "w");
    if (!arquivo) return 0;

    fputs("[\n", arquivo);
    for (int i = 0; i < base->quantidade; i++) {
        Registro *registro = &base->registros[i];
        RegistroPresencaArquivo *entrada = buscarPresencaArquivo(marcacoes, total_marcacoes, registro);
        int marcado = entrada && entrada->marcado && presencaValida(entrada->presenca);
        char *presenca = marcado ? entrada->presenca : "";

        fprintf(arquivo, "  {\"identificador\": \"%d\",\"nome\": ", registro->identificador);
        escreverTextoJson(arquivo, registro->nome);
        fprintf(arquivo, ",\"presenca\": ");
        escreverTextoJson(arquivo, presenca);
        fprintf(arquivo, ",\"marcado\": %s}%s\n", marcado ? "true" : "false", i < base->quantidade - 1 ? "," : "");
    }
    fputs("]\n", arquivo);
    fclose(arquivo);

    if (caminho_saida && tamanho_saida > 0) {
        snprintf(caminho_saida, tamanho_saida, "%s", caminho);
    }
    return 1;
}

void imprimirListaChamadasJson(char *pasta) {
    char caminhos[MAX_EVENTOS][TAM_CAMINHO], nomes[MAX_EVENTOS][TAM_NOME];
    int quantidade_arquivos = listarArquivosJson(pasta, caminhos, nomes, MAX_EVENTOS);

    char *nomes_ordenados[MAX_EVENTOS];
    for (int i = 0; i < quantidade_arquivos; i++) nomes_ordenados[i] = nomes[i];
    qsort(nomes_ordenados, quantidade_arquivos, sizeof(char*), compararTextos);

    fputs("[\n", stdout);
    for (int indice_nome = 0; indice_nome < quantidade_arquivos; indice_nome++) {
        int indice_arquivo = -1;
        for (int i = 0; i < quantidade_arquivos; i++)
            if (strcmp(nomes_ordenados[indice_nome], nomes[i]) == 0) { indice_arquivo = i; break; }
        if (indice_arquivo == -1) continue;

        char nome_evento[TAM_NOME];
        snprintf(nome_evento, sizeof nome_evento, "%s", nomes_ordenados[indice_nome]);
        char *ponto = strrchr(nome_evento, '.');
        if (ponto) *ponto = '\0';

        char data[16] = "";
        obterDataArquivo(caminhos[indice_arquivo], data, sizeof data);

        fputs("  {\"nome\": ", stdout);
        escreverTextoJson(stdout, nome_evento);
        fputs(",\"data\": ", stdout);
        escreverTextoJson(stdout, data);
        fputs("}", stdout);
        if (indice_nome < quantidade_arquivos - 1) fputs(",\n", stdout);
    }
    fputs("\n]\n", stdout);
}

void carregarEventosPresenca(DadosPresenca *dados, char *pasta, int total_membros) {
    memset(dados, 0, sizeof *dados);
    
    char caminhos[MAX_EVENTOS][TAM_CAMINHO], nomes[MAX_EVENTOS][TAM_NOME];
    int quantidade_arquivos = listarArquivosJson(pasta, caminhos, nomes, MAX_EVENTOS);
    if (quantidade_arquivos <= 0) return;
    
    char *nomes_ordenados[MAX_EVENTOS];
    for (int i = 0; i < quantidade_arquivos; i++) nomes_ordenados[i] = nomes[i];
    qsort(nomes_ordenados, quantidade_arquivos, sizeof(char*), compararTextos);
    
    for (int indice_nome = 0; indice_nome < quantidade_arquivos && dados->quantidade_eventos < MAX_EVENTOS; indice_nome++) {
        int indice_arquivo = -1;
        for (int i = 0; i < quantidade_arquivos; i++)
            if (strcmp(nomes_ordenados[indice_nome], nomes[i]) == 0) { indice_arquivo = i; break; }
        if (indice_arquivo == -1) continue;
        
        RegistroPresencaArquivo registros_presenca[MAX_PASTAS];
        int total_registros_arquivo = 0;
        carregarPresencas(caminhos[indice_arquivo], registros_presenca, MAX_PASTAS, &total_registros_arquivo);
        if (total_registros_arquivo <= 0) continue;
        
        EventoPresenca *evento = &dados->eventos[dados->quantidade_eventos];
        extrairData(nomes_ordenados[indice_nome], evento->data_evento);
        strncpy(evento->nome_evento, nomes_ordenados[indice_nome], TAM_NOME - 1);
        evento->nome_evento[TAM_NOME - 1] = '\0';
        
        char *ponto = strrchr(evento->nome_evento, '.');
        if (ponto) *ponto = '\0';
        
        evento->total_registros = total_membros > total_registros_arquivo ? total_membros : total_registros_arquivo;
        for (int i = 0; i < total_registros_arquivo; i++) {
            if (strcmp(registros_presenca[i].presenca, "Sim") == 0) evento->total_presentes++;
        }
        evento->total_ausentes = evento->total_registros - evento->total_presentes;
        if (evento->total_ausentes < 0) evento->total_ausentes = 0;
        dados->quantidade_eventos++;
    }
}

void calcularPresencaPorPessoa(AnalisePresenca *analise, BaseDados *base, FiltroAtivo filtro, char *pasta) {
    DadosPresenca dados = analise->dados;
    *analise = (AnalisePresenca){ .dados = dados };
    
    for (int i = 0; i < base->quantidade && analise->quantidade_individuos < MAX_PASTAS; i++) {
        Registro *registro = &base->registros[i];
        if (filtro == FILTRO_ATIVO_SIM && registro->ativo != ATIVO_SIM) continue;
        if (filtro == FILTRO_ATIVO_NAO && registro->ativo != ATIVO_NAO) continue;
        if (filtro == FILTRO_ATIVO_VISITANTE && registro->ativo != ATIVO_VISITANTE) continue;
        
        PresencaIndividual *presenca_individual = &analise->individuos[analise->quantidade_individuos++];
        strncpy(presenca_individual->nome, registro->nome, TAM_NOME - 1);
        presenca_individual->nome[TAM_NOME - 1] = '\0';
        
        int distancia_do_evento_mais_recente = 0;
        for (int indice_evento = analise->dados.quantidade_eventos - 1; indice_evento >= 0; indice_evento--) {
            EventoPresenca *evento = &analise->dados.eventos[indice_evento];
            presenca_individual->eventos_total++;
            
            char caminho_evento[TAM_CAMINHO];
            if (snprintf(caminho_evento, sizeof caminho_evento, "%s\\%s.json", pasta, evento->nome_evento) < 0) continue;
            
            RegistroPresencaArquivo registros_presenca[MAX_PASTAS];
            int total_registros_arquivo = 0;
            carregarPresencas(caminho_evento, registros_presenca, MAX_PASTAS, &total_registros_arquivo);
            
            for (int indice_presenca = 0; indice_presenca < total_registros_arquivo; indice_presenca++) {
                if (registros_presenca[indice_presenca].identificador == registro->identificador &&
                    strcmp(registros_presenca[indice_presenca].presenca, "Sim") == 0) {
                    presenca_individual->presencas_total++;
                    if (distancia_do_evento_mais_recente == 0) presenca_individual->presencas_ultimo = 1;
                    if (distancia_do_evento_mais_recente == 1) presenca_individual->presencas_penultimo = 1;
                    break;
                }
            }
            distancia_do_evento_mais_recente++;
        }
    }
}

void calcularMediaPresenca(AnalisePresenca *analise) {
    if (analise->quantidade_individuos == 0) return;
    
    double soma_ultimo = 0;
    double soma_penultimo = 0;
    double soma_geral = 0;
    int total_individuos = analise->quantidade_individuos;
    
    for (int i = 0; i < total_individuos; i++) {
        PresencaIndividual *pessoa = &analise->individuos[i];
        soma_ultimo += pessoa->presencas_ultimo;
        soma_penultimo += pessoa->presencas_penultimo;
        if (pessoa->eventos_total > 0) soma_geral += (double)pessoa->presencas_total / pessoa->eventos_total * 100.0;
    }
    
    analise->media_presenca_ultimo = soma_ultimo / total_individuos * 100.0;
    analise->media_presenca_penultimo = soma_penultimo / total_individuos * 100.0;
    analise->media_presenca_geral = soma_geral / total_individuos;
}

/* ========== IDADE ========== */
DistribuicaoIdade calcularDistribuicao(BaseDados *base, FiltroAtivo filtro) {
    DistribuicaoIdade distribuicao = {0};
    for (int i = 0; i < base->quantidade; i++) {
        Registro *registro = &base->registros[i];
        if (filtro == FILTRO_ATIVO_SIM && registro->ativo != ATIVO_SIM) continue;
        if (filtro == FILTRO_ATIVO_NAO && registro->ativo != ATIVO_NAO) continue;
        if (filtro == FILTRO_ATIVO_VISITANTE && registro->ativo != ATIVO_VISITANTE) continue;
        switch (registro->idade) {
            case IDADE_CRIANCA: distribuicao.crianca++; break;
            case IDADE_JOVEM:   distribuicao.jovem++;   break;
            case IDADE_ADULTO:  distribuicao.adulto++;  break;
            case IDADE_JUVENIL: distribuicao.juvenil++; break;
            default:            distribuicao.indefinido++;
        }
    }
    return distribuicao;
}

int contarFiltro(BaseDados *base, FiltroAtivo filtro) {
    int total = 0;
    for (int i = 0; i < base->quantidade; i++) {
        if (filtro == FILTRO_ATIVO_SIM && base->registros[i].ativo == ATIVO_SIM) total++;
        if (filtro == FILTRO_ATIVO_NAO && base->registros[i].ativo == ATIVO_NAO) total++;
        if (filtro == FILTRO_ATIVO_VISITANTE && base->registros[i].ativo == ATIVO_VISITANTE) total++;
    }
    return total;
}

void calcularAnaliseIdade(AnaliseIdade *analise, BaseDados *base, char *pasta) {
    memset(analise, 0, sizeof *analise);
    
    analise->dist_ativos = calcularDistribuicao(base, FILTRO_ATIVO_SIM);
    analise->dist_inativos = calcularDistribuicao(base, FILTRO_ATIVO_NAO);
    analise->dist_geral = calcularDistribuicao(base, FILTRO_TODOS);
    analise->total_ativos = contarFiltro(base, FILTRO_ATIVO_SIM);
    analise->total_inativos = contarFiltro(base, FILTRO_ATIVO_NAO);
    
    DadosPresenca dados_presenca;
    carregarEventosPresenca(&dados_presenca, pasta, base->quantidade);
    
    int quantidade_snapshots = dados_presenca.quantidade_eventos > 0 ? dados_presenca.quantidade_eventos : 1;
    for (int indice_snapshot = 0; indice_snapshot < quantidade_snapshots; indice_snapshot++) {
        SnapshotIdade *snapshot = &analise->evolucao.snapshots[indice_snapshot];
        strncpy(snapshot->data, dados_presenca.quantidade_eventos > 0 ? dados_presenca.eventos[indice_snapshot].data_evento : "Atual", sizeof snapshot->data - 1);
        snapshot->data[sizeof snapshot->data - 1] = '\0';
        snapshot->distribuicao = analise->dist_geral;
        snapshot->total_registros = base->quantidade;
        analise->evolucao.quantidade_snapshots++;
    }
}

typedef struct {
    char *comando, *nome, *nomeAnterior, *ativo, *idade, *arquivo, *pasta, *saida, *chamada, *entrada;
} Parametros;

Parametros lerArgumentos(int argc, char *argv[]) {
    Parametros parametros = {
        .comando = argc > 1 ? argv[1] : NULL,
        .arquivo = ARQUIVO_PADRAO,
        .pasta = PASTA_PRESENCAS,
        .saida = RELATORIO_PADRAO
    };
    
    for (int i = 2; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--nome") == 0)               parametros.nome         = argv[i + 1];
        else if (strcmp(argv[i], "--nome-anterior") == 0) parametros.nomeAnterior = argv[i + 1];
        else if (strcmp(argv[i], "--ativo") == 0)         parametros.ativo        = argv[i + 1];
        else if (strcmp(argv[i], "--idade") == 0)         parametros.idade        = argv[i + 1];
        else if (strcmp(argv[i], "--arquivo") == 0)       parametros.arquivo      = argv[i + 1];
        else if (strcmp(argv[i], "--presencas") == 0)     parametros.pasta        = argv[i + 1];
        else if (strcmp(argv[i], "--saida") == 0)         parametros.saida        = argv[i + 1];
        else if (strcmp(argv[i], "--chamada") == 0)       parametros.chamada      = argv[i + 1];
        else if (strcmp(argv[i], "--entrada") == 0)       parametros.entrada      = argv[i + 1];
    }
    return parametros;
}

int validarArgumentos(Parametros *parametros) {
    if (!parametros || !parametros->comando) return 0;
    if (strcmp(parametros->comando, "listar") != 0 &&
        strcmp(parametros->comando, "adicionar") != 0 &&
        strcmp(parametros->comando, "editar") != 0 &&
        strcmp(parametros->comando, "remover") != 0 &&
        strcmp(parametros->comando, "listar-chamadas") != 0 &&
        strcmp(parametros->comando, "carregar-chamada") != 0 &&
        strcmp(parametros->comando, "ordenar-presenca") != 0 &&
        strcmp(parametros->comando, "salvar-chamada") != 0 &&
        strcmp(parametros->comando, "relatorio") != 0) return 0;
    
    if (strcmp(parametros->comando, "adicionar") == 0)
        return parametros->nome && parametros->ativo && parametros->idade;
    if (strcmp(parametros->comando, "editar") == 0)
        return (parametros->nome || parametros->nomeAnterior) && parametros->ativo && parametros->idade;
    if (strcmp(parametros->comando, "remover") == 0)
        return parametros->nome != NULL;
    if (strcmp(parametros->comando, "carregar-chamada") == 0)
        return parametros->chamada != NULL;
    if (strcmp(parametros->comando, "salvar-chamada") == 0)
        return parametros->chamada != NULL && parametros->entrada != NULL;
    
    return 1;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    Parametros parametros = lerArgumentos(argc, argv);
    if (!validarArgumentos(&parametros)) return 1;

    if (strcmp(parametros.comando, "listar") == 0) {
        BaseDados base;
        carregarBaseDados(&base, parametros.arquivo);
        imprimirLista(&base);
        return 0;
    }

    if (strcmp(parametros.comando, "listar-chamadas") == 0) {
        imprimirListaChamadasJson(parametros.pasta);
        return 0;
    }

    BaseDados base;
    carregarBaseDados(&base, parametros.arquivo);

    if (strcmp(parametros.comando, "carregar-chamada") == 0) {
        char caminho_chamada[TAM_CAMINHO];
        montarCaminhoChamada(caminho_chamada, sizeof caminho_chamada, parametros.pasta, parametros.chamada);
        imprimirLinhasPresenca(&base, caminho_chamada);
        return 0;
    }

    if (strcmp(parametros.comando, "ordenar-presenca") == 0) {
        imprimirLinhasPresenca(&base, parametros.entrada);
        return 0;
    }

    if (strcmp(parametros.comando, "salvar-chamada") == 0) {
        char caminho_salvo[TAM_CAMINHO] = "";
        if (!salvarChamadaPresenca(&base, parametros.pasta, parametros.chamada, parametros.entrada, caminho_salvo, sizeof caminho_salvo))
            return 1;
        printf("{\"status\": \"salvo\", \"arquivo\": ");
        escreverTextoJson(stdout, caminho_salvo);
        printf("}\n");
        return 0;
    }

    if (strcmp(parametros.comando, "adicionar") == 0) {
        if (!adicionarRegistro(&base, parametros.nome, CONVERTER_ATIVO(parametros.ativo), textoParaFaixaIdade(parametros.idade)))
            return 1;
        if (!salvarBaseDados(&base, parametros.arquivo)) return 1;
        imprimirRegistro(buscarRegistro(&base, parametros.nome));
        return 0;
    }

    if (strcmp(parametros.comando, "editar") == 0) {
        char *nome_referencia = parametros.nomeAnterior ? parametros.nomeAnterior : parametros.nome;
        if (!editarRegistro(&base, nome_referencia, parametros.nome, CONVERTER_ATIVO(parametros.ativo), textoParaFaixaIdade(parametros.idade)))
            return 1;
        if (!salvarBaseDados(&base, parametros.arquivo)) return 1;
        imprimirRegistro(buscarRegistro(&base, parametros.nome && *parametros.nome ? parametros.nome : nome_referencia));
        return 0;
    }

    if (strcmp(parametros.comando, "remover") == 0) {
        if (!removerRegistro(&base, parametros.nome)) return 1;
        if (!salvarBaseDados(&base, parametros.arquivo)) return 1;
        JSON_SUCESSO("Removido com sucesso");
        return 0;
    }

    if (strcmp(parametros.comando, "relatorio") == 0) {
        AnaliseIdade analiseIdade;
        AnalisePresenca analisePresenca;
        carregarEventosPresenca(&analisePresenca.dados, parametros.pasta, base.quantidade);
        calcularAnaliseIdade(&analiseIdade, &base, parametros.pasta);
        calcularPresencaPorPessoa(&analisePresenca, &base, FILTRO_TODOS, parametros.pasta);
        calcularMediaPresenca(&analisePresenca);
        if (!GerarPlanilha(parametros.saida, &analiseIdade, &analisePresenca)) return 1;
        printf("{\"status\": \"gerado\", \"arquivo\": \"%s\"}\n", parametros.saida);
        return 0;
    }
    return 1;
}
