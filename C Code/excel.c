#include "projeto.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ======= GERADOR ZIP MINIMO (sem compressao) =======
typedef struct {
    char     *buf;
    uint32_t  cap;
    uint32_t  pos;
    uint32_t  crc_table[256];
} EscritorZip;

void InicializarZip(EscritorZip *z, char *buf, uint32_t cap) {
    z->buf = buf; z->cap = cap; z->pos = 0;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) c = (c >> 1) ^ ((c & 1) ? 0xEDB88320 : 0);
        z->crc_table[i] = c;
    }
}

uint32_t CalcularCRC32(uint8_t *d, uint32_t len, uint32_t *tbl) {
    uint32_t c = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) c = tbl[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

void Escrever16(char *b, uint16_t v) { b[0]=(char)v; b[1]=(char)(v>>8); }
void Escrever32(char *b, uint32_t v) { b[0]=(char)v; b[1]=(char)(v>>8); b[2]=(char)(v>>16); b[3]=(char)(v>>24); }

int AdicionarArquivoZip(EscritorZip *z, const char *name, const char *data, uint32_t dlen) {
    uint32_t nlen = (uint32_t)strlen(name);
    uint32_t crc = CalcularCRC32((uint8_t*)data, dlen, z->crc_table);

    if (z->pos + 30 + nlen + dlen > z->cap) return 0;
    Escrever32(z->buf + z->pos, 0x04034B50); z->pos += 4;
    Escrever16(z->buf + z->pos, 20);         z->pos += 2;
    Escrever16(z->buf + z->pos, 0);          z->pos += 2;
    Escrever16(z->buf + z->pos, 0);          z->pos += 2;
    Escrever16(z->buf + z->pos, 0);          z->pos += 2;
    Escrever16(z->buf + z->pos, 0);          z->pos += 2;
    Escrever32(z->buf + z->pos, crc);        z->pos += 4;
    Escrever32(z->buf + z->pos, dlen);       z->pos += 4;
    Escrever32(z->buf + z->pos, dlen);       z->pos += 4;
    Escrever16(z->buf + z->pos, nlen);       z->pos += 2;
    Escrever16(z->buf + z->pos, 0);          z->pos += 2;
    memcpy(z->buf + z->pos, name, nlen); z->pos += nlen;
    memcpy(z->buf + z->pos, data, dlen); z->pos += dlen;
    return 1;
}

typedef struct { char name[256]; uint32_t crc, off, nlen, dlen; } EntradaZip;

int FinalizarZip(EscritorZip *z, EntradaZip *entries, int nent) {
    uint32_t cd_off = z->pos;
    uint32_t cd_size = 0;

    for (int i = 0; i < nent; i++) {
        EntradaZip *e = &entries[i];
        uint32_t sz = 46 + e->nlen;
        if (z->pos + sz > z->cap) return 0;
        Escrever32(z->buf + z->pos, 0x02014B50); z->pos += 4;
        Escrever16(z->buf + z->pos, 20);          z->pos += 2;
        Escrever16(z->buf + z->pos, 20);          z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever32(z->buf + z->pos, e->crc);      z->pos += 4;
        Escrever32(z->buf + z->pos, e->dlen);     z->pos += 4;
        Escrever32(z->buf + z->pos, e->dlen);     z->pos += 4;
        Escrever16(z->buf + z->pos, e->nlen);     z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever16(z->buf + z->pos, 0);           z->pos += 2;
        Escrever32(z->buf + z->pos, 0);           z->pos += 4;
        Escrever32(z->buf + z->pos, e->off);      z->pos += 4;
        memcpy(z->buf + z->pos, e->name, e->nlen); z->pos += e->nlen;
        cd_size += sz;
    }

    if (z->pos + 22 > z->cap) return 0;
    Escrever32(z->buf + z->pos, 0x06054B50); z->pos += 4;
    Escrever16(z->buf + z->pos, 0);           z->pos += 2;
    Escrever16(z->buf + z->pos, 0);           z->pos += 2;
    Escrever16(z->buf + z->pos, (uint16_t)nent); z->pos += 2;
    Escrever16(z->buf + z->pos, (uint16_t)nent); z->pos += 2;
    Escrever32(z->buf + z->pos, cd_size);     z->pos += 4;
    Escrever32(z->buf + z->pos, cd_off);      z->pos += 4;
    Escrever16(z->buf + z->pos, 0);           z->pos += 2;
    return 1;
}

// ======= BASE XML =======
#define CABECALHO_XML "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"

// ======= CONTEUDO XML DAS FOLHAS =======
void CabecalhoFolha(char *b, int mx) {
    snprintf(b, mx, CABECALHO_XML
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"\n"
        "  xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
        "<sheetPr/><dimension ref=\"A1\"/><sheetViews><sheetView workbookViewId=\"0\"/></sheetViews>\n"
        "<sheetFormatPr defaultRowHeight=\"15\"/>\n<cols>\n"
        "<col min=\"1\" max=\"1\" width=\"28\"/>\n<col min=\"2\" max=\"2\" width=\"16\"/>\n"
        "<col min=\"3\" max=\"3\" width=\"16\"/>\n<col min=\"4\" max=\"4\" width=\"16\"/>\n"
        "<col min=\"5\" max=\"5\" width=\"16\"/>\n<col min=\"6\" max=\"6\" width=\"16\"/>\n"
        "<col min=\"7\" max=\"7\" width=\"18\"/>\n<col min=\"8\" max=\"8\" width=\"18\"/>\n"
        "</cols><sheetData>\n");
}

void RodapeFolha(char *b, int mx) {
    snprintf(b, mx, "</sheetData><pageMargins left=\"0.7\" right=\"0.7\" top=\"0.75\" bottom=\"0.75\"/>\n</worksheet>");
}

/* Folha 1: Resumo */
int GerarResumoXml(char *b, int mx, AnaliseIdade *ai, AnalisePresenca *ap) {
    int pos = 0;
    CabecalhoFolha(b, mx); pos = strlen(b);
    pos += snprintf(b+pos, mx-pos, "<row ht=\"28\" customHeight=\"1\"><c r=\"A1\" s=\"1\" t=\"inlineStr\"><is><t>Relatorio de Analise - Idade e Presencas</t></is></c></row>\n<row ht=\"8\"><c r=\"A2\"/></row>\n");
    int row = 3;

    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"2\" t=\"inlineStr\"><is><t>Distribuicao de Idade (Ativos)</t></is></c></row>\n", row);
    row++;
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"3\" t=\"inlineStr\"><is><t>Faixa</t></is></c><c r=\"B%d\" s=\"3\" t=\"inlineStr\"><is><t>Quantidade</t></is></c><c r=\"C%d\" s=\"3\" t=\"inlineStr\"><is><t>Percentagem</t></is></c></row>\n", row, row, row);
    row++;
    int total_ativos = ai->total_ativos > 0 ? ai->total_ativos : 1;
    char *faixas[] = {"Crianca","Jovem","Adulto","Juvenil"};
    int quantidades[] = {ai->dist_ativos.crianca, ai->dist_ativos.jovem, ai->dist_ativos.adulto, ai->dist_ativos.juvenil};
    for (int i=0; i<4; i++) {
        pos += snprintf(b+pos, mx-pos, "<row>");
        pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"4\" t=\"inlineStr\"><is><t>%s</t></is></c>", row, faixas[i]);
        pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"5\"><v>%d</v></c>", row, quantidades[i]);
        pos += snprintf(b+pos, mx-pos, "<c r=\"C%d\" s=\"6\"><v>%.6f</v></c>", row, (double)quantidades[i] / total_ativos);
        pos += snprintf(b+pos, mx-pos, "</row>\n"); row++;
    }
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\">");
    pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"7\" t=\"inlineStr\"><is><t>Total Ativos</t></is></c>", row);
    pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"7\"><v>%d</v></c>", row, ai->total_ativos);
    pos += snprintf(b+pos, mx-pos, "<c r=\"C%d\" s=\"7\"><v>1</v></c>", row);
    pos += snprintf(b+pos, mx-pos, "</row>\n"); row += 2;

    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"2\" t=\"inlineStr\"><is><t>Estatisticas de Presenca</t></is></c></row>\n", row);
    row++;
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"3\" t=\"inlineStr\"><is><t>Metrica</t></is></c><c r=\"B%d\" s=\"3\" t=\"inlineStr\"><is><t>Valor</t></is></c></row>\n", row, row);
    row++;
    char *metricas[] = {"Media Ultimo Evento","Media Penultimo Evento","Media Geral","Total Eventos"};
    double valores_metricas[] = {ap->media_presenca_ultimo/100.0, ap->media_presenca_penultimo/100.0, ap->media_presenca_geral/100.0, (double)ap->dados.quantidade_eventos};
    int estilos_metricas[] = {6,6,6,5};
    for (int i=0; i<4; i++) {
        pos += snprintf(b+pos, mx-pos, "<row>");
        pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"4\" t=\"inlineStr\"><is><t>%s</t></is></c>", row, metricas[i]);
        if (i < 3) pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"%d\"><v>%.6f</v></c>", row, estilos_metricas[i], valores_metricas[i]);
        else pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"%d\"><v>%.0f</v></c>", row, estilos_metricas[i], valores_metricas[i]);
        pos += snprintf(b+pos, mx-pos, "</row>\n"); row++;
    }

    RodapeFolha(b+pos, mx-pos);
    return 1;
}

/* Folha 2: Distribuicao Idade */
int GerarDistribuicaoIdadeXml(char *b, int mx, AnaliseIdade *ai) {
    int pos = 0;
    CabecalhoFolha(b, mx); pos = strlen(b);
    pos += snprintf(b+pos, mx-pos, "<row ht=\"28\"><c r=\"A1\" s=\"1\" t=\"inlineStr\"><is><t>Distribuicao de Idade por Status</t></is></c></row>\n<row ht=\"8\"><c r=\"A2\"/></row>\n");
    int row = 3;
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"3\" t=\"inlineStr\"><is><t>Faixa Etaria</t></is></c><c r=\"B%d\" s=\"3\" t=\"inlineStr\"><is><t>Ativos</t></is></c><c r=\"C%d\" s=\"3\" t=\"inlineStr\"><is><t>Inativos</t></is></c><c r=\"D%d\" s=\"3\" t=\"inlineStr\"><is><t>Total</t></is></c></row>\n", row,row,row,row);
    row++;
    char *faixas[] = {"Crianca","Jovem","Adulto","Juvenil"};
    int quantidades_ativos[] = {ai->dist_ativos.crianca,ai->dist_ativos.jovem,ai->dist_ativos.adulto,ai->dist_ativos.juvenil};
    int quantidades_inativos[] = {ai->dist_inativos.crianca,ai->dist_inativos.jovem,ai->dist_inativos.adulto,ai->dist_inativos.juvenil};
    for (int i=0; i<4; i++) {
        pos += snprintf(b+pos, mx-pos, "<row>");
        pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"4\" t=\"inlineStr\"><is><t>%s</t></is></c>", row, faixas[i]);
        pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"5\"><v>%d</v></c><c r=\"C%d\" s=\"5\"><v>%d</v></c><c r=\"D%d\" s=\"5\"><v>%d</v></c>", row,quantidades_ativos[i],row,quantidades_inativos[i],row,quantidades_ativos[i]+quantidades_inativos[i]);
        pos += snprintf(b+pos, mx-pos, "</row>\n"); row++;
    }
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\">");
    pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"7\" t=\"inlineStr\"><is><t>Total</t></is></c>", row);
    pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"7\"><v>%d</v></c><c r=\"C%d\" s=\"7\"><v>%d</v></c><c r=\"D%d\" s=\"7\"><v>%d</v></c>", row,ai->total_ativos,row,ai->total_inativos,row,ai->total_ativos+ai->total_inativos);
    pos += snprintf(b+pos, mx-pos, "</row>\n"); row += 2;

    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"2\" t=\"inlineStr\"><is><t>Percentagem por Faixa (Ativos)</t></is></c></row>\n", row);
    row++;
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"3\" t=\"inlineStr\"><is><t>Faixa</t></is></c><c r=\"B%d\" s=\"3\" t=\"inlineStr\"><is><t>%% Ativos</t></is></c><c r=\"C%d\" s=\"3\" t=\"inlineStr\"><is><t>%% Inativos</t></is></c></row>\n", row,row,row);
    row++;
    for (int i=0; i<4; i++) {
        double percentual_ativos = ai->total_ativos>0 ? (double)quantidades_ativos[i]/ai->total_ativos : 0;
        double percentual_inativos = ai->total_inativos>0 ? (double)quantidades_inativos[i]/ai->total_inativos : 0;
        pos += snprintf(b+pos, mx-pos, "<row>");
        pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"4\" t=\"inlineStr\"><is><t>%s</t></is></c>", row, faixas[i]);
        pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"6\"><v>%.6f</v></c><c r=\"C%d\" s=\"6\"><v>%.6f</v></c>", row,percentual_ativos,row,percentual_inativos);
        pos += snprintf(b+pos, mx-pos, "</row>\n"); row++;
    }

    RodapeFolha(b+pos, mx-pos);
    return 1;
}

/* Folha 3: Evolucao Idade */
int GerarEvolucaoIdadeXml(char *b, int mx, AnaliseIdade *ai) {
    int pos = 0;
    CabecalhoFolha(b, mx); pos = strlen(b);
    pos += snprintf(b+pos, mx-pos, "<row ht=\"28\"><c r=\"A1\" s=\"1\" t=\"inlineStr\"><is><t>Evolucao de Idade ao Longo do Tempo</t></is></c></row>\n<row ht=\"8\"><c r=\"A2\"/></row>\n");
    int row = 3;
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"3\" t=\"inlineStr\"><is><t>Data</t></is></c><c r=\"B%d\" s=\"3\" t=\"inlineStr\"><is><t>Crianca</t></is></c><c r=\"C%d\" s=\"3\" t=\"inlineStr\"><is><t>Jovem</t></is></c><c r=\"D%d\" s=\"3\" t=\"inlineStr\"><is><t>Adulto</t></is></c><c r=\"E%d\" s=\"3\" t=\"inlineStr\"><is><t>Juvenil</t></is></c><c r=\"F%d\" s=\"3\" t=\"inlineStr\"><is><t>Total</t></is></c></row>\n", row,row,row,row,row,row);
    row++;
    for (int i=0; i<ai->evolucao.quantidade_snapshots; i++) {
        SnapshotIdade *s = &ai->evolucao.snapshots[i];
        pos += snprintf(b+pos, mx-pos, "<row>");
        pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"4\" t=\"inlineStr\"><is><t>%s</t></is></c>", row, s->data);
        pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"5\"><v>%d</v></c><c r=\"C%d\" s=\"5\"><v>%d</v></c><c r=\"D%d\" s=\"5\"><v>%d</v></c><c r=\"E%d\" s=\"5\"><v>%d</v></c><c r=\"F%d\" s=\"5\"><v>%d</v></c>",
            row,s->distribuicao.crianca, row,s->distribuicao.jovem, row,s->distribuicao.adulto, row,s->distribuicao.juvenil, row,s->total_registros);
        pos += snprintf(b+pos, mx-pos, "</row>\n"); row++;
    }
    RodapeFolha(b+pos, mx-pos);
    return 1;
}

/* Folha 4: Presenca Anual */
int GerarPresencaAnualXml(char *b, int mx, AnalisePresenca *ap) {
    int pos = 0;
    CabecalhoFolha(b, mx); pos = strlen(b);
    pos += snprintf(b+pos, mx-pos, "<row ht=\"28\"><c r=\"A1\" s=\"1\" t=\"inlineStr\"><is><t>Analise Anual de Presencas</t></is></c></row>\n<row ht=\"8\"><c r=\"A2\"/></row>\n");
    int row = 3;
    pos += snprintf(b+pos, mx-pos, "<row ht=\"22\"><c r=\"A%d\" s=\"3\" t=\"inlineStr\"><is><t>Periodo</t></is></c><c r=\"B%d\" s=\"3\" t=\"inlineStr\"><is><t>Eventos</t></is></c><c r=\"C%d\" s=\"3\" t=\"inlineStr\"><is><t>Presentes</t></is></c><c r=\"D%d\" s=\"3\" t=\"inlineStr\"><is><t>Ausentes</t></is></c><c r=\"E%d\" s=\"3\" t=\"inlineStr\"><is><t>Total</t></is></c><c r=\"F%d\" s=\"3\" t=\"inlineStr\"><is><t>%%</t></is></c><c r=\"G%d\" s=\"3\" t=\"inlineStr\"><is><t>Media</t></is></c></row>\n", row,row,row,row,row,row,row);
    row++;

    typedef struct { char mes[8]; int presentes, ausentes, registros, eventos; } Mes;
    Mes meses[12]; int quantidade_meses = 0;
    for (int i=0; i<ap->dados.quantidade_eventos; i++) {
        EventoPresenca *ev = &ap->dados.eventos[i];
        char mes_texto[8] = "";
        if (strlen(ev->data_evento)>=7 && ev->data_evento[4]=='-') { strncpy(mes_texto, ev->data_evento, 7); mes_texto[7]=0; }
        else snprintf(mes_texto, sizeof mes_texto, "Evento");
        int indice_mes = -1;
        for (int m=0; m<quantidade_meses; m++) if (strcmp(meses[m].mes,mes_texto)==0) { indice_mes=m; break; }
        if (indice_mes==-1 && quantidade_meses<12) { indice_mes=quantidade_meses++; strncpy(meses[indice_mes].mes,mes_texto,7); meses[indice_mes].mes[7]=0; meses[indice_mes].presentes=0; meses[indice_mes].ausentes=0; meses[indice_mes].registros=0; meses[indice_mes].eventos=0; }
        if (indice_mes>=0) { meses[indice_mes].presentes+=ev->total_presentes; meses[indice_mes].ausentes+=ev->total_ausentes; meses[indice_mes].registros+=ev->total_registros; meses[indice_mes].eventos++; }
    }

    int soma_presentes = 0, soma_registros = 0;
    for (int m=0; m<quantidade_meses; m++) {
        double percentual = meses[m].registros>0 ? (double)meses[m].presentes/meses[m].registros : 0;
        soma_presentes += meses[m].presentes; soma_registros += meses[m].registros;
        double media = soma_registros>0 ? (double)soma_presentes/soma_registros : 0;
        pos += snprintf(b+pos, mx-pos, "<row>");
        pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"4\" t=\"inlineStr\"><is><t>%s</t></is></c>", row, meses[m].mes);
        pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"5\"><v>%d</v></c><c r=\"C%d\" s=\"5\"><v>%d</v></c><c r=\"D%d\" s=\"5\"><v>%d</v></c><c r=\"E%d\" s=\"5\"><v>%d</v></c>", row,meses[m].eventos, row,meses[m].presentes, row,meses[m].ausentes, row,meses[m].registros);
        pos += snprintf(b+pos, mx-pos, "<c r=\"F%d\" s=\"6\"><v>%.6f</v></c>", row, percentual);
        pos += snprintf(b+pos, mx-pos, "<c r=\"G%d\" s=\"8\"><v>%.6f</v></c>", row, media);
        pos += snprintf(b+pos, mx-pos, "</row>\n"); row++;
    }

    if (quantidade_meses > 0) {
        double media_anual = soma_registros>0 ? (double)soma_presentes/soma_registros : 0;
        pos += snprintf(b+pos, mx-pos, "<row ht=\"22\">");
        pos += snprintf(b+pos, mx-pos, "<c r=\"A%d\" s=\"7\" t=\"inlineStr\"><is><t>MEDIA ANUAL</t></is></c>", row);
        pos += snprintf(b+pos, mx-pos, "<c r=\"B%d\" s=\"7\"/><c r=\"C%d\" s=\"7\"/><c r=\"D%d\" s=\"7\"/><c r=\"E%d\" s=\"7\"/>", row,row,row,row);
        pos += snprintf(b+pos, mx-pos, "<c r=\"F%d\" s=\"7\"><v>%.6f</v></c>", row, media_anual);
        pos += snprintf(b+pos, mx-pos, "<c r=\"G%d\" s=\"7\"><v>%.6f</v></c>", row, media_anual);
        pos += snprintf(b+pos, mx-pos, "</row>\n");
    }

    RodapeFolha(b+pos, mx-pos);
    return 1;
}

// ======= ESTILOS =======
void GerarEstilosXml(char *b, int mx) {
    snprintf(b, mx, CABECALHO_XML
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
        "<fonts count=\"4\">\n"
        "  <font><sz val=\"11\"/><name val=\"Calibri\"/></font>\n"
        "  <font><sz val=\"14\"/><b/><color rgb=\"FF1F4E79\"/><name val=\"Calibri\"/></font>\n"
        "  <font><sz val=\"12\"/><b/><color rgb=\"FF2E75B6\"/><name val=\"Calibri\"/></font>\n"
        "  <font><sz val=\"11\"/><b/><color rgb=\"FFFFFFFF\"/><name val=\"Calibri\"/></font>\n"
        "</fonts>\n"
        "<fills count=\"3\">\n"
        "  <fill><patternFill patternType=\"none\"/></fill>\n"
        "  <fill><patternFill patternType=\"gray125\"/></fill>\n"
        "  <fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF2E75B6\"/></patternFill></fill>\n"
        "</fills>\n"
        "<borders count=\"2\">\n"
        "  <border/>\n"
        "  <border><bottom style=\"thin\"><color rgb=\"FFD9E2F3\"/></bottom></border>\n"
        "</borders>\n"
        "<cellXfs count=\"9\">\n"
        "  <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n"
        "  <xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" applyFont=\"1\"/>\n"
        "  <xf numFmtId=\"0\" fontId=\"2\" fillId=\"0\" borderId=\"0\" applyFont=\"1\"/>\n"
        "  <xf numFmtId=\"0\" fontId=\"3\" fillId=\"2\" borderId=\"1\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\"/>\n"
        "  <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" applyBorder=\"1\"/>\n"
        "  <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" applyBorder=\"1\" applyNumberFormat=\"1\"/>\n"
        "  <xf numFmtId=\"10\" fontId=\"0\" fillId=\"0\" borderId=\"0\" applyNumberFormat=\"1\"/>\n"
        "  <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n"
        "  <xf numFmtId=\"10\" fontId=\"0\" fillId=\"0\" borderId=\"0\" applyNumberFormat=\"1\"/>\n"
        "</cellXfs>\n"
        "</styleSheet>");
}

// ======= GERACAO DA PLANILHA =======
int GerarPlanilha(const char *caminho, AnaliseIdade *analise_idade, AnalisePresenca *analise_presenca) {
    if (!caminho || !analise_idade || !analise_presenca) return 0;

    uint32_t capacidade_zip = 512 * 1024;
    char *zip = malloc(capacidade_zip);
    if (!zip) return 0;

    EscritorZip escritor_zip;
    InicializarZip(&escritor_zip, zip, capacidade_zip);

    int tamanho_xml = 64 * 1024;
    char *xml = malloc(tamanho_xml);
    if (!xml) { free(zip); return 0; }

    EntradaZip entradas[16];
    int quantidade_entradas = 0;

    #define ADICIONAR_XML(nome_entrada, geracao) do { \
    memset(xml, 0, tamanho_xml); \
    (geracao); \
    uint32_t tamanho_atual = (uint32_t)strlen(xml); \
    entradas[quantidade_entradas].crc = CalcularCRC32((uint8_t*)xml, tamanho_atual, escritor_zip.crc_table); \
    entradas[quantidade_entradas].off = escritor_zip.pos; \
    entradas[quantidade_entradas].nlen = (uint32_t)strlen(nome_entrada); \
    entradas[quantidade_entradas].dlen = tamanho_atual; \
    strncpy(entradas[quantidade_entradas].name, nome_entrada, sizeof(entradas[quantidade_entradas].name) - 1); \
    entradas[quantidade_entradas].name[sizeof(entradas[quantidade_entradas].name) - 1] = '\0'; \
    if (!AdicionarArquivoZip(&escritor_zip, nome_entrada, xml, tamanho_atual)) { free(xml); free(zip); return 0; } \
    quantidade_entradas++; \
    } while (0)

    ADICIONAR_XML("[Content_Types].xml", snprintf(xml, tamanho_xml, CABECALHO_XML
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
        "<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
        "<Override PartName=\"/xl/worksheets/sheet3.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
        "<Override PartName=\"/xl/worksheets/sheet4.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n"
        "</Types>"));

    ADICIONAR_XML("_rels/.rels", snprintf(xml, tamanho_xml, CABECALHO_XML
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
        "</Relationships>"));

    ADICIONAR_XML("xl/_rels/workbook.xml.rels", snprintf(xml, tamanho_xml, CABECALHO_XML
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>\n"
        "<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet3.xml\"/>\n"
        "<Relationship Id=\"rId5\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet4.xml\"/>\n"
        "</Relationships>"));

    ADICIONAR_XML("xl/workbook.xml", snprintf(xml, tamanho_xml, CABECALHO_XML
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"\n"
        "  xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
        "<sheets>\n"
        "<sheet name=\"Resumo\" sheetId=\"1\" r:id=\"rId2\"/>\n"
        "<sheet name=\"Distribuicao Idade\" sheetId=\"2\" r:id=\"rId3\"/>\n"
        "<sheet name=\"Evolucao Idade\" sheetId=\"3\" r:id=\"rId4\"/>\n"
        "<sheet name=\"Presenca Anual\" sheetId=\"4\" r:id=\"rId5\"/>\n"
        "</sheets>\n</workbook>"));

    ADICIONAR_XML("xl/styles.xml", GerarEstilosXml(xml, tamanho_xml));

    ADICIONAR_XML("xl/worksheets/sheet1.xml", GerarResumoXml(xml, tamanho_xml, analise_idade, analise_presenca));
    ADICIONAR_XML("xl/worksheets/sheet2.xml", GerarDistribuicaoIdadeXml(xml, tamanho_xml, analise_idade));
    ADICIONAR_XML("xl/worksheets/sheet3.xml", GerarEvolucaoIdadeXml(xml, tamanho_xml, analise_idade));
    ADICIONAR_XML("xl/worksheets/sheet4.xml", GerarPresencaAnualXml(xml, tamanho_xml, analise_presenca));

    #undef ADICIONAR_XML

    free(xml);

    if (!FinalizarZip(&escritor_zip, entradas, quantidade_entradas)) { free(zip); return 0; }

    FILE *f = NULL;
#ifdef _WIN32
    {
        int nw = MultiByteToWideChar(CP_UTF8, 0, caminho, -1, NULL, 0);
        wchar_t *wc = malloc((nw + 1) * sizeof(wchar_t));
        if (wc) {
            MultiByteToWideChar(CP_UTF8, 0, caminho, -1, wc, nw);
            f = _wfopen(wc, L"wb");
            free(wc);
        }
    }
#else
    f = fopen(caminho, "wb");
#endif
    if (!f) { free(zip); return 0; }

    size_t bytes_escritos = fwrite(zip, 1, escritor_zip.pos, f);
    fclose(f);
    free(zip);

    return bytes_escritos == escritor_zip.pos;
}
