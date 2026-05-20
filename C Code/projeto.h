#ifndef PROJETO_H
#define PROJETO_H

#include <stdio.h>

/* ======= Constantes ======= */
#define MAX_REGISTROS 2000
#define MAX_EVENTOS   4000
#define MAX_PASTAS    200
#define TAM_CAMINHO   512
#define TAM_NOME      256
#define TAM_BUFFER    512

/* ======= Enums ======= */
typedef enum {
    IDADE_INDEFINIDO = 0,
    IDADE_JUVENIL,
    IDADE_ADULTO,
    IDADE_JOVEM,
    IDADE_CRIANCA
} FaixaIdade;

typedef enum {
    ATIVO_NAO = 0,
    ATIVO_SIM,
    ATIVO_VISITANTE
} AtivoMembro;

typedef enum {
    FILTRO_TODOS = 0,
    FILTRO_ATIVO_SIM,
    FILTRO_ATIVO_NAO,
    FILTRO_ATIVO_VISITANTE
} FiltroAtivo;

/* ======= Structs ======= */
typedef struct {
    int         identificador;
    AtivoMembro ativo;
    FaixaIdade  idade;
    char        nome[TAM_NOME];
} Registro;

typedef struct {
    Registro registros[MAX_REGISTROS];
    int      quantidade;
} BaseDados;

/* ======= Structs Presenca ======= */
typedef struct {
    char nome[TAM_NOME];
    int  identificador;
    char presenca[16];
    int  marcado;
} RegistroPresencaArquivo;

typedef struct {
    char nome_evento[TAM_NOME];
    char data_evento[16];
    int  total_presentes;
    int  total_ausentes;
    int  total_registros;
} EventoPresenca;

typedef struct {
    EventoPresenca eventos[MAX_EVENTOS];
    int            quantidade_eventos;
} DadosPresenca;

typedef struct {
    char nome[TAM_NOME];
    int  presencas_ultimo;
    int  presencas_penultimo;
    int  presencas_total;
    int  eventos_total;
} PresencaIndividual;

typedef struct {
    PresencaIndividual individuos[MAX_PASTAS];
    int                quantidade_individuos;
    double             media_presenca_ultimo;
    double             media_presenca_penultimo;
    double             media_presenca_geral;
    DadosPresenca      dados;
} AnalisePresenca;

/* ======= Structs Idade ======= */
typedef struct {
    int crianca;
    int jovem;
    int adulto;
    int juvenil;
    int indefinido;
} DistribuicaoIdade;

typedef struct {
    char data[16];
    DistribuicaoIdade distribuicao;
    int total_registros;
} SnapshotIdade;

typedef struct {
    SnapshotIdade snapshots[MAX_EVENTOS];
    int           quantidade_snapshots;
} EvolucaoIdade;

typedef struct {
    DistribuicaoIdade dist_ativos;
    DistribuicaoIdade dist_inativos;
    DistribuicaoIdade dist_geral;
    EvolucaoIdade     evolucao;
    int               total_ativos;
    int               total_inativos;
} AnaliseIdade;

/* ======= Planilha ======= */
int GerarPlanilha(const char *caminho_saida, AnaliseIdade *idade, AnalisePresenca *presenca);

#endif
