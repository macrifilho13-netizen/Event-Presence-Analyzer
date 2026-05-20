Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ====== Configurações e estado ======
$executavelRegistro = Join-Path $PSScriptRoot "registrador.exe"
$pastaPresencas = Join-Path $PSScriptRoot "presencas"

$script:registros = @()
$script:presencas = @{}
$script:presencasOriginais = @{}
$script:arquivoPresenca = $null
$script:chamadaNova = $false
$script:colunaOrdenacao = -1
$script:ordemCrescente = $true
$script:botoes = @{}
$script:preenchendoFormulario = $false
$global:_copiaPresencas = @{}
$global:_arquivoCopia = $null

# ====== Cores / fontes ======
$cores = @{
  Verde       = [System.Drawing.Color]::FromArgb(34, 197, 94);   Azul    = [System.Drawing.Color]::FromArgb(59, 130, 246)
  Verm        = [System.Drawing.Color]::FromArgb(239, 68, 68);   Amarelo = [System.Drawing.Color]::FromArgb(245, 158, 11)
  Cinza       = [System.Drawing.Color]::FromArgb(156, 163, 175); Fundo   = [System.Drawing.Color]::FromArgb(249, 250, 251)
  Branco      = [System.Drawing.Color]::FromArgb(255, 255, 255); Texto   = [System.Drawing.Color]::FromArgb(17, 24, 39)
  TL          = [System.Drawing.Color]::FromArgb(107, 114, 128); Borda   = [System.Drawing.Color]::FromArgb(229, 231, 235)
  Placeholder = [System.Drawing.Color]::FromArgb(209, 213, 219)
}

$fontes = @{
  Titulo = New-Object System.Drawing.Font("Segoe UI", 14, [System.Drawing.FontStyle]::Bold)
  Normal = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Regular)
  Bold   = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold)
  Btn    = New-Object System.Drawing.Font("Segoe UI", 12, [System.Drawing.FontStyle]::Bold)
  Badge  = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)
}

# ====== Backend helpers ======
function ExecutarBackend {
  param([string]$Operacao, [hashtable]$Parametros = @{})
  $argumentos = @($Operacao)
  foreach ($k in $Parametros.Keys) { $argumentos += "--$k"; $argumentos += $Parametros[$k] }
  $saida = (& $executavelRegistro $argumentos 2>&1) -join "`n"
  if ($LASTEXITCODE -eq 0 -and $saida -match '^\s*[\[{]') { $saida | ConvertFrom-Json -ErrorAction SilentlyContinue }
}

function CarregarRegistros {
  $retorno = ExecutarBackend "listar"
  if ($retorno) { $script:registros = @($retorno) } else { $script:registros = @() }
}

# ====== Mensagens / confirmação ======
function MostrarMensagem {
  param([string]$titulo, [string]$mensagem)
  [System.Windows.Forms.MessageBox]::Show($mensagem, $titulo,
    [System.Windows.Forms.MessageBoxButtons]::OK,
    [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null
}

function ConfirmarAcao {
  param([string]$pergunta)
  $resp = [System.Windows.Forms.MessageBox]::Show($pergunta, "Confirmar",
    [System.Windows.Forms.MessageBoxButtons]::YesNo,
    [System.Windows.Forms.MessageBoxIcon]::Question)
  return $resp -eq [System.Windows.Forms.DialogResult]::Yes
}

function PerguntarSalvarAlteracoesChamada {
  [System.Windows.Forms.MessageBox]::Show("Deseja salvar as alteracoes realizadas nesta chamada?", "Salvar alteracoes",
    [System.Windows.Forms.MessageBoxButtons]::YesNoCancel,
    [System.Windows.Forms.MessageBoxIcon]::Question)
}

# ====== UI helpers ======
function New-LabelEx([string]$Text, $X, $Y, $W, $H, $Font, $Color) {
  $lbl = New-Object System.Windows.Forms.Label
  $lbl.Text = $Text; $lbl.Location = New-Object System.Drawing.Point($X, $Y)
  $lbl.Size = New-Object System.Drawing.Size($W, $H); $lbl.Font = $Font; $lbl.ForeColor = $Color
  return $lbl
}

function AjustarCorEscala([System.Drawing.Color]$cor, [int]$delta) {
  $r = [Math]::Min([Math]::Max($cor.R + $delta, 0), 255)
  $g = [Math]::Min([Math]::Max($cor.G + $delta, 0), 255)
  $b = [Math]::Min([Math]::Max($cor.B + $delta, 0), 255)
  return [System.Drawing.Color]::FromArgb($r, $g, $b)
}

function New-ButtonEx([string]$Text, $X, $Y, $W, $H, $Fore, $Back, $OnClick, $Font) {
  $btn = New-Object System.Windows.Forms.Button
  $btn.Text = $Text; $btn.Location = New-Object System.Drawing.Point($X, $Y)
  $btn.Size = New-Object System.Drawing.Size($W, $H); $btn.BackColor = $Back; $btn.ForeColor = $Fore
  $btn.Font = if ($Font) { $Font } else { $fontes.Btn }
  $btn.FlatStyle = "Flat"; $btn.FlatAppearance.BorderSize = 0
  $btn.Cursor = "Hand"; $btn.UseVisualStyleBackColor = $false
  $btn.Tag = $Back
  $btn.Add_MouseEnter({
      param($sender, $e)
      if ($sender -and $sender.Tag -is [System.Drawing.Color]) { $sender.BackColor = AjustarCorEscala $sender.Tag 15 }
    })
  $btn.Add_MouseLeave({
      param($sender, $e)
      if ($sender -and $sender.Tag -is [System.Drawing.Color]) { $sender.BackColor = $sender.Tag }
    })
  if ($OnClick) { $btn.Add_Click($OnClick) }
  return $btn
}

function New-PanelEx($BackColor = $cores.Branco) {
  $p = New-Object System.Windows.Forms.Panel
  $p.BackColor = $BackColor; $p.BorderStyle = "None"; $p.Padding = New-Object System.Windows.Forms.Padding(10)
  return $p
}

function New-TextBoxEx($X, $Y, $W, $H, $Font) {
  $txt = New-Object System.Windows.Forms.TextBox
  $txt.Location = New-Object System.Drawing.Point($X, $Y); $txt.Size = New-Object System.Drawing.Size($W, $H)
  $txt.Font = $Font; $txt.BorderStyle = "FixedSingle"; $txt.BackColor = $cores.Branco; $txt.ForeColor = $cores.Texto
  return $txt
}

function New-ComboBoxEx($X, $Y, $W, $H, $Font) {
  $combo = New-Object System.Windows.Forms.ComboBox
  $combo.Location = New-Object System.Drawing.Point($X, $Y); $combo.Size = New-Object System.Drawing.Size($W, $H)
  $combo.Font = $Font; $combo.DropDownStyle = "DropDownList"; $combo.FlatStyle = "Flat"
  $combo.BackColor = $cores.Branco; $combo.ForeColor = $cores.Texto
  return $combo
}

function Add-PlaceholderText($textbox, [string]$placeholder) {
  $textbox.Tag = @{ Placeholder = $placeholder; Active = $true }
  $textbox.Text = $placeholder; $textbox.ForeColor = $cores.Placeholder
  $textbox.Add_GotFocus({
      if ($textbox.Text -eq $textbox.Tag.Placeholder) {
        $textbox.Text = ""; $textbox.ForeColor = $cores.Texto
        $textbox.Tag.Active = $false
      }
    })
  $textbox.Add_LostFocus({
      if ([string]::IsNullOrWhiteSpace($textbox.Text)) {
        $textbox.Text = $textbox.Tag.Placeholder; $textbox.ForeColor = $cores.Placeholder
        $textbox.Tag.Active = $true
      }
    })
}

function New-DataGridEx([string[]]$Columns, $X, $Y, $W, $H) {
  $t = New-Object System.Windows.Forms.DataGridView
  $t.Location = New-Object System.Drawing.Point($X, $Y); $t.Size = New-Object System.Drawing.Size($W, $H)
  $t.BackgroundColor = $cores.Fundo; $t.ReadOnly = $true; $t.SelectionMode = "FullRowSelect"
  $t.AllowUserToAddRows = $false; $t.AllowUserToDeleteRows = $false; $t.AllowUserToResizeRows = $false
  $t.AutoSizeColumnsMode = "Fill"; $t.ColumnCount = $Columns.Count
  for ($i = 0; $i -lt $Columns.Count; $i++) { $t.Columns[$i].Name = $Columns[$i] }
  $t.RowHeadersVisible = $false; $t.EnableHeadersVisualStyles = $false; $t.GridColor = $cores.Borda
  $t.ColumnHeadersHeight = 34; $t.RowTemplate.Height = 30
  $hdrStyle = New-Object System.Windows.Forms.DataGridViewCellStyle
  $hdrStyle.BackColor = $cores.Azul; $hdrStyle.ForeColor = $cores.Branco; $hdrStyle.Font = $fontes.Bold
  $t.ColumnHeadersDefaultCellStyle = $hdrStyle
  $cellStyle = New-Object System.Windows.Forms.DataGridViewCellStyle
  $cellStyle.BackColor = $cores.Branco; $cellStyle.Font = $fontes.Normal
  $cellStyle.SelectionBackColor = $cores.Azul; $cellStyle.SelectionForeColor = $cores.Branco
  $altStyle = New-Object System.Windows.Forms.DataGridViewCellStyle
  $altStyle.BackColor = [System.Drawing.Color]::FromArgb(249, 250, 251); $altStyle.Font = $fontes.Normal
  $altStyle.SelectionBackColor = $cores.Azul; $altStyle.SelectionForeColor = $cores.Branco
  $t.DefaultCellStyle = $cellStyle; $t.AlternatingRowsDefaultCellStyle = $altStyle
  $t.BorderStyle = "None"
  return $t
}

# ====== GERENCIAMENTO DE ESTADO DO VISITANTE ======
function AtualizarEstadoVisitante {
  if ($chkVisitante.Checked) {
    $comboAtivo.SelectedIndex = 1  # "Nao"
    $comboAtivo.Enabled = $false
    $comboAtivo.BackColor = $cores.Borda
  }
  else {
    $comboAtivo.Enabled = $true
    $comboAtivo.BackColor = $cores.Branco
  }
}

# ====== NAVEGACAO ======
function AbrirPaginaPresenca {
  $paginaMembros.Visible = $false; $paginaPresenca.Visible = $true
  $painelInicio.Visible = $true; $painelChamada.Visible = $false; $painelListaPresencas.Visible = $true
  $textoChamada.Clear(); $listaCartoes.Controls.Clear()
  $script:arquivoPresenca = $null; $script:presencas = @{}; $script:presencasOriginais = @{}; $script:botoes = @{}; $script:chamadaNova = $false
  if ($barraProgresso) { $barraProgresso.Width = 0 }
  if ($rotuloContador) { $rotuloContador.Text = "0 / 0 membros" }
  CarregarRegistros; AtualizarListaChamadas
}

function AbrirMembros {
  $global:_copiaPresencas = @{}; $global:_arquivoCopia = $null
  $paginaPresenca.Visible = $false; $paginaMembros.Visible = $true
  CarregarRegistros; AtualizarTabelaRegistros
}

function AbrirMembrosDuranteChamada {
  $global:_copiaPresencas = @{}
  foreach ($k in $script:presencas.Keys) { $global:_copiaPresencas[$k] = $script:presencas[$k] }
  $global:_arquivoCopia = $script:arquivoPresenca
  $paginaPresenca.Visible = $false; $paginaMembros.Visible = $true
  CarregarRegistros; AtualizarTabelaRegistros
}

function VoltarParaPresenca {
  if ($null -ne $global:_arquivoCopia) {
    $script:presencas = $global:_copiaPresencas; $script:arquivoPresenca = $global:_arquivoCopia
    $global:_copiaPresencas = @{}; $global:_arquivoCopia = $null
  }
  $paginaMembros.Visible = $false; $paginaPresenca.Visible = $true
  if ($painelChamada.Visible) { AtualizarCartoesPresenca }
}

# ====== TABELA E ORDENAÇÃO ======
function AtualizarTabelaRegistros {
  $tabelaRegistros.Rows.Clear()
  foreach ($r in $script:registros) {
    $bg = switch ($r.ativo) {
      "Sim" { [System.Drawing.Color]::FromArgb(212, 245, 225) }
      "Visitante" { [System.Drawing.Color]::FromArgb(229, 190, 254) }
      default { [System.Drawing.Color]::FromArgb(252, 217, 212) }
    }
    $i = $tabelaRegistros.Rows.Add($r.nome, $r.ativo, $r.idade)
    $tabelaRegistros.Rows[$i].Cells[1].Style.BackColor = $bg
  }
}

function OrdenarTabelaRegistros([int]$Coluna) {
  if ($script:colunaOrdenacao -eq $Coluna) { $script:ordemCrescente = -not $script:ordemCrescente } else { $script:colunaOrdenacao = $Coluna; $script:ordemCrescente = $true }
  $seta = if ($script:ordemCrescente) { " $([char]0x25B2)" } else { " $([char]0x25BC)" }
  switch ($Coluna) {
    0 { $script:registros = if ($script:ordemCrescente) { $script:registros | Sort-Object { $_.nome } } else { $script:registros | Sort-Object { $_.nome } -Descending } }
    1 {
      $ordem = if ($script:ordemCrescente) { @("Sim", "Visitante", "Nao") } else { @("Nao", "Visitante", "Sim") }
      $script:registros = $script:registros | Sort-Object { $ordem.IndexOf($_.ativo) }
    }
  }
  AtualizarTabelaRegistros
  for ($i = 0; $i -lt $tabelaRegistros.Columns.Count; $i++) {
    $n = $tabelaRegistros.Columns[$i].Name
    $tabelaRegistros.Columns[$i].HeaderText = if ($i -eq $script:colunaOrdenacao) { "$n$seta" } else { $n }
  }
}

# ====== BUSCA / SELECAO DE MEMBROS ======
function EncontrarRegistroPorNome([string]$Nome) {
  if ([string]::IsNullOrWhiteSpace($Nome)) { return $null }
  $alvo = $Nome.Trim()
  foreach ($r in $script:registros) {
    if ([System.String]::Equals([string]$r.nome, $alvo, [System.StringComparison]::CurrentCultureIgnoreCase)) { return $r }
  }
  return $null
}

function EncontrarLinhaRegistroPorNome([string]$Nome) {
  if ([string]::IsNullOrWhiteSpace($Nome) -or -not $tabelaRegistros) { return $null }
  $alvo = $Nome.Trim()
  foreach ($linha in $tabelaRegistros.Rows) {
    if ($linha.IsNewRow) { continue }
    if ([System.String]::Equals([string]$linha.Cells[0].Value, $alvo, [System.StringComparison]::CurrentCultureIgnoreCase)) { return $linha }
  }
  return $null
}

function SelecionarLinhaRegistro($Linha) {
  if (-not $Linha -or -not $tabelaRegistros) { return }
  $tabelaRegistros.ClearSelection()
  $Linha.Selected = $true
  $tabelaRegistros.CurrentCell = $Linha.Cells[0]
}

function PreencherFormularioComRegistro($Registro, [bool]$AtualizarNome = $true) {
  if (-not $Registro) { return }
  $script:preenchendoFormulario = $true
  try {
    if ($AtualizarNome) { $textoNome.Text = [string]$Registro.nome }
    $comboIdade.Text = [string]$Registro.idade
    $ativoValue = [string]$Registro.ativo
    if ($ativoValue -eq "Visitante") {
      $chkVisitante.Checked = $true
      $comboAtivo.SelectedIndex = 1
    }
    else {
      $chkVisitante.Checked = $false
      $comboAtivo.Text = $ativoValue
    }
    AtualizarEstadoVisitante
  }
  finally {
    $script:preenchendoFormulario = $false
  }
}

function SelecionarRegistroDigitado {
  if ($script:preenchendoFormulario -or -not $tabelaRegistros) { return }
  $registro = EncontrarRegistroPorNome $textoNome.Text
  if (-not $registro) { return }
  $linha = EncontrarLinhaRegistroPorNome $registro.nome
  if ($linha) { SelecionarLinhaRegistro $linha }
  PreencherFormularioComRegistro $registro $false
}

function ObterNomeRegistroParaEdicao {
  if ($tabelaRegistros.SelectedRows.Count -gt 0) { return [string]$tabelaRegistros.SelectedRows[0].Cells[0].Value }
  $registro = EncontrarRegistroPorNome $textoNome.Text
  if ($registro) { return [string]$registro.nome }
  return $null
}

function ObterNomeRegistroParaRemocao {
  $nomeDigitado = $textoNome.Text.Trim()
  if (-not [string]::IsNullOrWhiteSpace($nomeDigitado)) {
    $registro = EncontrarRegistroPorNome $nomeDigitado
    if ($registro) { return [string]$registro.nome }
    return $null
  }
  if ($tabelaRegistros.SelectedRows.Count -gt 0) { return [string]$tabelaRegistros.SelectedRows[0].Cells[0].Value }
  return $null
}

# ====== CRUD ======
function ExecutarAcaoRegistro([string]$Tipo) {
  $nome = $textoNome.Text.Trim(); $idade = $comboIdade.SelectedItem
  
  # Determinar ativo baseado no checkbox
  if ($chkVisitante.Checked) {
    $ativo = "Visitante"
  }
  else {
    $ativo = $comboAtivo.SelectedItem
  }
  
  $ok = $false
  
  switch ($Tipo) {
    "adicionar" {
      if ([string]::IsNullOrWhiteSpace($nome)) { MostrarMensagem "Aviso" "Nome nao pode estar vazio!"; return }
      $ok = [bool](ExecutarBackend "adicionar" @{ nome = $nome; ativo = $ativo; idade = $idade })
      if ($ok) { MostrarMensagem "Sucesso" "Adicionado!" }
    }
    "editar" {
      if ([string]::IsNullOrWhiteSpace($nome)) { MostrarMensagem "Aviso" "Digite ou selecione um registro!"; return }
      $nomeAnterior = ObterNomeRegistroParaEdicao
      if ([string]::IsNullOrWhiteSpace($nomeAnterior)) { MostrarMensagem "Aviso" "Selecione na lista ou digite um nome existente!"; return }
      if (-not (ConfirmarAcao "Editar?")) { return }
      $ok = [bool](ExecutarBackend "editar" @{ "nome-anterior" = $nomeAnterior; nome = $nome; ativo = $ativo; idade = $idade })
      if ($ok) { MostrarMensagem "Sucesso" "Editado!" }
    }
    "remover" {
      $nomeSelecionado = ObterNomeRegistroParaRemocao
      if ([string]::IsNullOrWhiteSpace($nomeSelecionado)) { MostrarMensagem "Aviso" "Selecione na lista ou digite um nome existente!"; return }
      if (-not (ConfirmarAcao "APAGAR '$nomeSelecionado'?")) { return }
      $ok = [bool](ExecutarBackend "remover" @{ nome = $nomeSelecionado })
      if ($ok) { MostrarMensagem "Sucesso" "Apagado!" }
    }
  }
  if ($ok) {
    if ($Tipo -eq "editar" -and -not [string]::IsNullOrWhiteSpace($nomeAnterior) -and $nome -ne $nomeAnterior) {
      foreach ($cache in @($script:presencas, $global:_copiaPresencas)) {
        if ($cache -and $cache.ContainsKey($nomeAnterior)) {
          $situacaoPresenca = $cache[$nomeAnterior]
          $cache.Remove($nomeAnterior)
          $cache[$nome] = $situacaoPresenca
        }
      }
    }
    elseif ($Tipo -eq "remover" -and -not [string]::IsNullOrWhiteSpace($nomeSelecionado)) {
      foreach ($cache in @($script:presencas, $global:_copiaPresencas)) {
        if ($cache -and $cache.ContainsKey($nomeSelecionado)) { $cache.Remove($nomeSelecionado) }
      }
    }
    CarregarRegistros; AtualizarTabelaRegistros
    if ($tabelaRegistros) { $tabelaRegistros.ClearSelection() }
    $textoNome.Clear(); $comboAtivo.SelectedIndex = 0; $comboIdade.SelectedIndex = 0; $chkVisitante.Checked = $false
    AtualizarEstadoVisitante
  }
}

# ====== PRESENCA ======
function AtualizarCartoesPresenca {
  $listaCartoes.Controls.Clear(); $script:botoes = @{}
  $cw = [Math]::Max(600, $listaCartoes.Width - 20)
  $filtro = if ($textoPesquisa.Tag -and $textoPesquisa.Tag.Placeholder -and $textoPesquisa.Text -eq $textoPesquisa.Tag.Placeholder) { "" } else { $textoPesquisa.Text }
  $filtro = $filtro.ToLower()
  $lista = ObterRegistrosPresencaOrdenados
  if ($filtro) { $lista = @($lista | Where-Object { $_.nome.ToLower().Contains($filtro) }) }
  foreach ($r in $lista) { CriarCartaoPresenca $r $cw }
  AtualizarContadorPresenca
}

function OrdenarRegistrosPresencaLocal($Lista) {
  $marcados = @()
  $naoMarcados = @()
  foreach ($r in $Lista) {
    if ($script:presencas.ContainsKey($r.nome)) { $marcados += $r } else { $naoMarcados += $r }
  }
  $marcados = @($marcados | Sort-Object { $_.nome })
  $naoMarcados = @($naoMarcados | Sort-Object { $_.nome })
  return @($marcados + $naoMarcados)
}

function SalvarMarcacoesTemporarias {
  $temporario = Join-Path ([System.IO.Path]::GetTempPath()) ("presencas_" + [System.IO.Path]::GetRandomFileName() + ".json")
  $l = @()
  foreach ($nome in $script:presencas.Keys) {
    $l += [ordered]@{ nome = $nome; presenca = $script:presencas[$nome]; marcado = $true }
  }
  $json = if ($l.Count -gt 0) { ($l | ConvertTo-Json -Depth 3).Trim() + "`n" } else { "[]`n" }
  [System.IO.File]::WriteAllText($temporario, $json, [System.Text.Encoding]::UTF8)
  return $temporario
}

function ObterRegistrosPresencaOrdenados {
  $temporario = SalvarMarcacoesTemporarias
  try {
    $retorno = ExecutarBackend "ordenar-presenca" @{ entrada = $temporario }
    if ($null -ne $retorno) { return @($retorno) }
  }
  finally {
    if (Test-Path $temporario) { Remove-Item $temporario -Force -ErrorAction SilentlyContinue }
  }
  return @(OrdenarRegistrosPresencaLocal $script:registros)
}

function CriarCartaoPresenca($Registro, [int]$Largura) {
  $nome = $Registro.nome; $presencaAtual = $script:presencas[$nome]
  $card = New-PanelEx; $card.Size = New-Object System.Drawing.Size($Largura, 60); $card.Margin = New-Object System.Windows.Forms.Padding(0, 2, 0, 2); $card.BorderStyle = "None"
  $barra = New-PanelEx; $barra.Size = New-Object System.Drawing.Size(6, 60)
  $barra.BackColor = if ($presencaAtual -eq "Sim") { $cores.Verde } elseif ($presencaAtual -eq "Nao") { $cores.Verm } else { $cores.Placeholder }
  $lblNome = New-LabelEx $Registro.nome 20 15 ([int]($Largura * 0.35)) 24 $fontes.Bold $cores.Texto
  $posicaoIdadeX = [int]($Largura * 0.35) + 20
  $corIdade = switch ($Registro.idade) { "Adulto" { "#3498DB" } "Juvenil" { "#9B59B6" } "Jovem" { "#E67E22" } default { "#00BCD4" } }
  
  if ($Registro.ativo -eq "Visitante") {
    $statusAtivo = "Visitante"
  }
  elseif ($Registro.ativo -eq "Sim") {
    $statusAtivo = "Ativo"
  }
  else {
    $statusAtivo = "Inativo"
  }
  
  $lblInfoText = "{0} - {1}" -f $Registro.idade, $statusAtivo
  $lblInfo = New-LabelEx $lblInfoText $posicaoIdadeX 17 160 22 $fontes.Badge $cores.Branco
  $lblInfo.TextAlign = "MiddleCenter"; $lblInfo.BackColor = $corIdade
  $larguraBotao = 70; $posNaoX = $Largura - $larguraBotao - 15; $posSimX = $posNaoX - $larguraBotao - 8
  $corSim = if ($presencaAtual -eq "Sim") { $cores.Verde } else { $cores.Borda }; $corNao = if ($presencaAtual -eq "Nao") { $cores.Verm } else { $cores.Borda }
  $botaoSim = New-ButtonEx "Sim" $posSimX 14 $larguraBotao 32 $cores.Branco $corSim $null
  $botaoNao = New-ButtonEx "Nao" $posNaoX 14 $larguraBotao 32 $cores.Branco $corNao $null
  if ($presencaAtual -ne "Sim") { $botaoSim.ForeColor = $cores.TL }; if ($presencaAtual -ne "Nao") { $botaoNao.ForeColor = $cores.TL }
  $script:botoes[$nome] = @{ i = $barra; s = $botaoSim; n = $botaoNao }
  $botaoSim.Tag = $nome
  $botaoNao.Tag = $nome
  $botaoSim.Add_Click({
      param($sender, $e)
      $origem = if ($sender) { $sender } else { $this }
      MarcarPresenca ([string]$origem.Tag) "Sim"
    })
  $botaoNao.Add_Click({
      param($sender, $e)
      $origem = if ($sender) { $sender } else { $this }
      MarcarPresenca ([string]$origem.Tag) "Nao"
    })
  $card.Controls.AddRange(@($barra, $lblNome, $lblInfo, $botaoSim, $botaoNao)); [void]$listaCartoes.Controls.Add($card)
}

function AtualizarBotoesPresenca([string]$Nome, [string]$Situacao) {
  $entrada = $script:botoes[$Nome]; if (-not $entrada) { return }
  if ($Situacao -eq "Sim") {
    $entrada.i.BackColor = $cores.Verde; $entrada.s.BackColor = $cores.Verde; $entrada.s.ForeColor = $cores.Branco
    $entrada.n.BackColor = $cores.Borda; $entrada.n.ForeColor = $cores.TL
  }
  else {
    $entrada.i.BackColor = $cores.Verm; $entrada.s.BackColor = $cores.Borda; $entrada.s.ForeColor = $cores.TL
    $entrada.n.BackColor = $cores.Verm; $entrada.n.ForeColor = $cores.Branco
  }
  $entrada.s.Invalidate(); $entrada.n.Invalidate(); $entrada.i.Invalidate()
}

function ReordenarCartoesPresenca {
  if (-not $listaCartoes -or $listaCartoes.Controls.Count -eq 0) { return }
  $filtro = $textoPesquisa.Text.ToLower()
  $ordenados = @(ObterRegistrosPresencaOrdenados)
  if ($filtro) { $ordenados = @($ordenados | Where-Object { $_.nome.ToLower().Contains($filtro) }) }

  $listaCartoes.SuspendLayout()
  try {
    for ($i = 0; $i -lt $ordenados.Count; $i++) {
      $nomeOrdenado = [string]$ordenados[$i].nome
      $entrada = $script:botoes[$nomeOrdenado]
      if ($entrada -and $entrada.i -and $entrada.i.Parent) {
        $listaCartoes.Controls.SetChildIndex($entrada.i.Parent, $i)
      }
    }
  }
  finally {
    $listaCartoes.ResumeLayout()
  }
}

function AtualizarContadorPresenca { 
  if (-not $rotuloContador) { return }
  $t = $script:registros.Count; $f = $script:presencas.Count
  $rotuloContador.Text = "$f / $t membros"
  $pct = if ($t -gt 0) { $f / $t } else { 0 }
  $barraProgresso.Width = [int]($pct * $barraFundo.Width)
}

function CopiarMapaPresencas($Origem) {
  $copia = @{}
  if ($Origem) {
    foreach ($k in $Origem.Keys) { $copia[$k] = $Origem[$k] }
  }
  return $copia
}

function RegistrarEstadoOriginalChamada {
  $script:presencasOriginais = CopiarMapaPresencas $script:presencas
}

function ChamadaTemAlteracoes {
  if ($script:chamadaNova) { return $true }
  if ($script:presencas.Count -ne $script:presencasOriginais.Count) { return $true }

  foreach ($k in $script:presencas.Keys) {
    if (-not $script:presencasOriginais.ContainsKey($k)) { return $true }
    if ([string]$script:presencasOriginais[$k] -ne [string]$script:presencas[$k]) { return $true }
  }

  return $false
}

function LimparChamadaAtual {
  $script:arquivoPresenca = $null
  $script:presencas = @{}
  $script:presencasOriginais = @{}
  $script:botoes = @{}
  $script:chamadaNova = $false
  $listaCartoes.Controls.Clear()
  $textoPesquisa.Clear()
  $painelChamada.Visible = $false
  $painelInicio.Visible = $true
  $painelListaPresencas.Visible = $true
  $textoChamada.Clear()
  $barraProgresso.Width = 0
  if ($rotuloContador) { $rotuloContador.Text = "0 / 0 membros" }
  AtualizarListaChamadas
}

function SalvarChamadaAtual {
  if (-not $script:arquivoPresenca) { MostrarMensagem "Erro" "Nenhum arquivo!"; return $false }
  $nomeChamada = [System.IO.Path]::GetFileNameWithoutExtension($script:arquivoPresenca)
  $temporario = SalvarMarcacoesTemporarias
  try {
    $retorno = ExecutarBackend "salvar-chamada" @{ presencas = $pastaPresencas; chamada = $nomeChamada; entrada = $temporario }
    if (-not $retorno) { MostrarMensagem "Erro" "Nao foi possivel salvar a chamada."; return $false }
    if ($retorno.arquivo) { $script:arquivoPresenca = [string]$retorno.arquivo }
  }
  finally {
    if (Test-Path $temporario) { Remove-Item $temporario -Force -ErrorAction SilentlyContinue }
  }
  $script:chamadaNova = $false
  RegistrarEstadoOriginalChamada
  return $true
}

function SairChamada {
  if (-not $script:arquivoPresenca) { LimparChamadaAtual; return }

  if (ChamadaTemAlteracoes) {
    $resposta = PerguntarSalvarAlteracoesChamada
    if ($resposta -eq [System.Windows.Forms.DialogResult]::Cancel) { return }
    if ($resposta -eq [System.Windows.Forms.DialogResult]::Yes) {
      if (-not (SalvarChamadaAtual)) { return }
      MostrarMensagem "Sucesso" "Salvo em '$([System.IO.Path]::GetFileName($script:arquivoPresenca))'!"
    }
  }

  LimparChamadaAtual
}

function MarcarPresenca([string]$Nome, [string]$Situacao) {
  if ([string]::IsNullOrWhiteSpace($Nome)) { return }
  if (-not $script:presencas) { $script:presencas = @{} }

  $script:presencas[$Nome] = $Situacao
  AtualizarBotoesPresenca $Nome $Situacao
  ReordenarCartoesPresenca
  AtualizarContadorPresenca
}

function IniciarChamada {
  $nomeChamada = if ($textoChamada.Tag -and $textoChamada.Tag.Placeholder -and $textoChamada.Text -eq $textoChamada.Tag.Placeholder) { "" } else { $textoChamada.Text.Trim() }
  if ([string]::IsNullOrWhiteSpace($nomeChamada)) { MostrarMensagem "Aviso" "Informe um nome!"; return }
  if (-not (Test-Path $pastaPresencas)) { mkdir $pastaPresencas -Force | Out-Null }
  $arquivoPresenca = Join-Path $pastaPresencas "$nomeChamada.json"
  if (Test-Path $arquivoPresenca) {
    if (ConfirmarAcao "Ja existe uma chamada com esse nome. Deseja abrir para editar?") { AbrirChamadaSalva $nomeChamada }
    return
  }
  $script:arquivoPresenca = $arquivoPresenca; $script:presencas = @{}; $script:chamadaNova = $true; RegistrarEstadoOriginalChamada; $painelChamada.Visible = $true; $painelInicio.Visible = $false; $painelListaPresencas.Visible = $false; AtualizarCartoesPresenca
}

function CarregarPresencasArquivo([string]$Arquivo) {
  $script:presencas = @{}
  $nomeChamada = [System.IO.Path]::GetFileNameWithoutExtension($Arquivo)
  $retorno = ExecutarBackend "carregar-chamada" @{ presencas = $pastaPresencas; chamada = $nomeChamada }
  if ($null -eq $retorno) {
    MostrarMensagem "Erro" "Nao foi possivel ler a chamada '$([System.IO.Path]::GetFileName($Arquivo))'."
    return $false
  }

  $script:registros = @($retorno)
  foreach ($item in $script:registros) {
    if ([bool]$item.marcado -and ([string]$item.presenca -eq "Sim" -or [string]$item.presenca -eq "Nao")) {
      $script:presencas[[string]$item.nome] = [string]$item.presenca
    }
  }

  return $true
}

function AbrirChamadaSalva([string]$NomeChamada) {
  if ([string]::IsNullOrWhiteSpace($NomeChamada)) { return }
  $arquivoPresenca = Join-Path $pastaPresencas "$NomeChamada.json"
  if (-not (Test-Path $arquivoPresenca)) {
    MostrarMensagem "Erro" "Chamada '$NomeChamada' nao encontrada."
    AtualizarListaChamadas
    return
  }

  CarregarRegistros
  if (-not (CarregarPresencasArquivo $arquivoPresenca)) { return }
  $script:arquivoPresenca = $arquivoPresenca
  $script:chamadaNova = $false
  RegistrarEstadoOriginalChamada
  $script:botoes = @{}
  $textoPesquisa.Clear()
  $textoChamada.Text = $NomeChamada
  $painelChamada.Visible = $true
  $painelInicio.Visible = $false
  $painelListaPresencas.Visible = $false
  AtualizarCartoesPresenca
}

function ObterNomeChamadaSelecionada {
  if ($tabelaChamadas.SelectedRows.Count -gt 0) { return [string]$tabelaChamadas.SelectedRows[0].Cells[0].Value }
  if ($tabelaChamadas.CurrentRow -and -not $tabelaChamadas.CurrentRow.IsNewRow) { return [string]$tabelaChamadas.CurrentRow.Cells[0].Value }
  return $null
}

function EditarChamadaSelecionada {
  $nomeChamada = ObterNomeChamadaSelecionada
  if ([string]::IsNullOrWhiteSpace($nomeChamada)) { MostrarMensagem "Aviso" "Selecione uma chamada salva!"; return }
  AbrirChamadaSalva $nomeChamada
}

function FinalizarChamada {
  if (-not (SalvarChamadaAtual)) { return }
  MostrarMensagem "Sucesso" "Salvo em '$([System.IO.Path]::GetFileName($script:arquivoPresenca))'!"
  LimparChamadaAtual
}

function AtualizarListaChamadas {
  $tabelaChamadas.Rows.Clear()
  $retorno = ExecutarBackend "listar-chamadas" @{ presencas = $pastaPresencas }
  if ($null -ne $retorno) {
    foreach ($chamada in @($retorno)) { [void]$tabelaChamadas.Rows.Add($chamada.nome, $chamada.data) }
    return
  }
  if (-not (Test-Path $pastaPresencas)) { return }
  Get-ChildItem $pastaPresencas "*.json" | Sort-Object -Property BaseName | ForEach-Object { [void]$tabelaChamadas.Rows.Add($_.BaseName, $_.CreationTime.ToString("dd/MM/yyyy")) }
}

function GerarPlanilha {
  $dlg = New-Object System.Windows.Forms.SaveFileDialog
  $dlg.Title = "Salvar relatorio da planilha"; $dlg.Filter = "Arquivo de planilha (*.xlsx)|*.xlsx"; $dlg.DefaultExt = "xlsx"; $dlg.AddExtension = $true
  $dlg.FileName = "relatorio.xlsx"; $dlg.InitialDirectory = $PSScriptRoot
  if ($dlg.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { return }
  $retorno = ExecutarBackend "relatorio" @{ presencas = $pastaPresencas; saida = $dlg.FileName }
  if ($retorno -and (Test-Path $dlg.FileName)) { MostrarMensagem "Sucesso" "Relatorio gerado em '$([System.IO.Path]::GetFileName($dlg.FileName))'!"; return }
  MostrarMensagem "Erro" "Nao foi possivel gerar o relatorio."
}

# ====== CONSTRUÇÃO DA JANELA ======
$form = New-Object System.Windows.Forms.Form
$form.Text = "Gerenciador de Membros"; $form.Size = New-Object System.Drawing.Size(1400, 900); $form.StartPosition = "CenterScreen"; $form.BackColor = $cores.Fundo
$larguraBase = 1380

$tooltip = New-Object System.Windows.Forms.ToolTip
$tooltip.AutoPopDelay = 5000
$tooltip.InitialDelay = 1000
$tooltip.ReshowDelay = 500

# ====== PAGINA PRESENCA ======
$paginaPresenca = New-PanelEx $cores.Fundo; $paginaPresenca.Dock = "Fill"

$topoPresenca = New-PanelEx $cores.Branco; $topoPresenca.Location = New-Object System.Drawing.Point(0, 0); $topoPresenca.Size = New-Object System.Drawing.Size($larguraBase, 56)
$botaoPlanilha = New-ButtonEx "PLANILHA" ($larguraBase - 320) 10 140 36 $cores.Branco $cores.Verde { GerarPlanilha }
$tooltip.SetToolTip($botaoPlanilha, "Gerar relatório em planilha Excel")
$botaoGerenciador = New-ButtonEx "MEMBROS" ($larguraBase - 160) 10 140 36 $cores.Branco $cores.Azul {
  try {
    if (Get-Command -Name AbrirMembrosDuranteChamada -CommandType Function -ErrorAction SilentlyContinue) {
      AbrirMembrosDuranteChamada
    }
    else {
      [System.Windows.Forms.MessageBox]::Show("Funcao AbrirMembrosDuranteChamada nao definida.", "Erro", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
    }
  }
  catch {
    [System.Windows.Forms.MessageBox]::Show("Erro ao chamar AbrirMembrosDuranteChamada:`n$($_.Exception.Message)", "Erro", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
  }
}
$tooltip.SetToolTip($botaoGerenciador, "Gerenciar membros")
$lblPres = New-LabelEx "CONTROLE DE PRESENCA" 16 12 300 28 $fontes.Titulo $cores.Azul; $lblPres.AutoSize = $true
$lblSubPres.AutoSize = $true
$topoPresenca.Controls.AddRange(@($lblPres, $lblSubPres, $botaoPlanilha, $botaoGerenciador)); $paginaPresenca.Controls.Add($topoPresenca)

$painelInicio = New-PanelEx; $painelInicio.Location = New-Object System.Drawing.Point(0, 64); $painelInicio.Size = New-Object System.Drawing.Size($larguraBase, 96)
$painelInicio.Controls.Add((New-LabelEx "Nome do evento:" 20 18 160 24 $fontes.Bold $cores.Texto))
$textoChamada = New-TextBoxEx 190 16 320 32 $fontes.Normal; $textoChamada.TextAlign = "Center"
$botaoIniciar = New-ButtonEx "INICIAR" 530 16 160 40 $cores.Branco $cores.Azul { IniciarChamada }
$tooltip.SetToolTip($botaoIniciar, "Iniciar nova chamada de presença")
$painelInicio.Controls.AddRange(@($textoChamada, $botaoIniciar)); $paginaPresenca.Controls.Add($painelInicio)

$painelListaPresencas = New-PanelEx; $painelListaPresencas.Location = New-Object System.Drawing.Point(0, 144); $painelListaPresencas.Size = New-Object System.Drawing.Size($larguraBase, 340)
$painelListaPresencas.Controls.Add((New-LabelEx "CHAMADAS SALVAS" 16 10 300 28 $fontes.Titulo $cores.Azul))
$botaoEditarChamada = New-ButtonEx "EDITAR" ($larguraBase - 160) 43 127 32 $cores.Branco $cores.Amarelo { EditarChamadaSelecionada }
$tooltip.SetToolTip($botaoEditarChamada, "Editar chamada selecionada")
$tabelaChamadas = New-DataGridEx @("Nome", "Data") 16 42 ($larguraBase - 32) 280
$tabelaChamadas.Add_CellDoubleClick({ if ($_.RowIndex -ge 0) { EditarChamadaSelecionada } })
$painelListaPresencas.Controls.AddRange(@($botaoEditarChamada, $tabelaChamadas)); $paginaPresenca.Controls.Add($painelListaPresencas)

# ====== PAINEL DE CHAMADA ======
$painelChamada = New-PanelEx; $painelChamada.Location = New-Object System.Drawing.Point(0, 64); $painelChamada.Size = New-Object System.Drawing.Size($larguraBase, 730); $painelChamada.Visible = $false
$topoChamada = New-PanelEx; $topoChamada.Location = New-Object System.Drawing.Point(0, 0); $topoChamada.Size = New-Object System.Drawing.Size($larguraBase, 52)
$botaoVoltarChamada = New-ButtonEx "VOLTAR" 12 10 100 34 $cores.Branco $cores.Cinza { SairChamada }
$tooltip.SetToolTip($botaoVoltarChamada, "Voltar para lista de chamadas")
$rotuloPesquisa = New-LabelEx "Pesquisar:" 120 14 80 20 $fontes.Bold $cores.Texto
$textoPesquisa = New-TextBoxEx 200 10 280 34 $fontes.Normal
Add-PlaceholderText $textoPesquisa "Pesquisar nomes de membros"
$textoPesquisa.Add_TextChanged({ AtualizarCartoesPresenca })
$rotuloContador = New-LabelEx "0 / 0 membros" 496 12 180 24 $fontes.Bold $cores.Azul; $rotuloContador.TextAlign = "MiddleLeft"
$barraFundo = New-PanelEx $cores.Borda; $barraFundo.Location = New-Object System.Drawing.Point(496, 36); $barraFundo.Size = New-Object System.Drawing.Size(300, 6)
$barraProgresso = New-PanelEx $cores.Verde; $barraProgresso.Location = New-Object System.Drawing.Point(0, 0); $barraProgresso.Size = New-Object System.Drawing.Size(0, 6)
$barraFundo.Controls.Add($barraProgresso)
$botaoFinalizar = New-ButtonEx "FINALIZAR" ($larguraBase - 150) 10 130 36 $cores.Branco $cores.Verde { FinalizarChamada }
$tooltip.SetToolTip($botaoFinalizar, "Finalizar e salvar chamada")
$topoChamada.Controls.AddRange(@($botaoVoltarChamada, $rotuloPesquisa, $textoPesquisa, $rotuloContador, $barraFundo, $botaoFinalizar))
$listaCartoes = New-Object System.Windows.Forms.FlowLayoutPanel; $listaCartoes.Location = New-Object System.Drawing.Point(0, 56); $listaCartoes.Size = New-Object System.Drawing.Size($larguraBase, 674); $listaCartoes.AutoScroll = $true; $listaCartoes.BackColor = $cores.Fundo; $listaCartoes.WrapContents = $false; $listaCartoes.FlowDirection = "TopDown"
$painelChamada.Controls.AddRange(@($topoChamada, $listaCartoes)); $paginaPresenca.Controls.Add($painelChamada)

# ====== PAGINA MEMBROS ======
$paginaMembros = New-PanelEx $cores.Fundo; $paginaMembros.Dock = "Fill"; $paginaMembros.Visible = $false

$topoMembros = New-PanelEx $cores.Branco; $topoMembros.Location = New-Object System.Drawing.Point(0, 0); $topoMembros.Size = New-Object System.Drawing.Size($larguraBase, 56)
$botaoVoltarMembros = New-ButtonEx "VOLTAR" 12 10 120 36 $cores.Branco $cores.Cinza { VoltarParaPresenca }
$tooltip.SetToolTip($botaoVoltarMembros, "Voltar para controle de presença")
$rotuloMembros = New-LabelEx "MEMBROS" 148 12 400 28 $fontes.Titulo $cores.Azul; $rotuloMembros.AutoSize = $true
$lblSubMembros.AutoSize = $true
$topoMembros.Controls.AddRange(@($botaoVoltarMembros, $rotuloMembros, $lblSubMembros)); $paginaMembros.Controls.Add($topoMembros)

# ====== TABELA DE MEMBROS ======
$tabelaRegistros = New-DataGridEx @("Nome", "Ativo", "Idade") 12 68 ($larguraBase - 24) 370
$tabelaRegistros.Add_CellClick({
    if ($tabelaRegistros.SelectedRows.Count -gt 0) {
      $r = $tabelaRegistros.SelectedRows[0]
      $registro = EncontrarRegistroPorNome $r.Cells[0].Value
      if ($registro) { PreencherFormularioComRegistro $registro }
    }
  })
$tabelaRegistros.Add_KeyDown({ if ($_.KeyCode -eq "Delete") { $_.Handled = $true; ExecutarAcaoRegistro "remover" } })
$tabelaRegistros.Add_ColumnHeaderMouseClick({ $c = $_.ColumnIndex; if ($c -ge 0 -and $c -le 1) { OrdenarTabelaRegistros $c } })
$paginaMembros.Controls.Add($tabelaRegistros)

# ====== FORMULARIO ======
$painelFormulario = New-PanelEx $cores.Branco; $painelFormulario.Location = New-Object System.Drawing.Point(12, 480); $painelFormulario.Size = New-Object System.Drawing.Size(620, 170)
$painelFormulario.Controls.Add((New-LabelEx "EDITAR / ADICIONAR" 16 8 300 24 $fontes.Titulo $cores.Azul))

$painelFormulario.Controls.Add((New-LabelEx "Nome:" 16 42 80 20 $fontes.Bold $cores.Texto))
$textoNome = New-Object System.Windows.Forms.TextBox; $textoNome.Location = New-Object System.Drawing.Point(100, 40); $textoNome.Size = New-Object System.Drawing.Size(500, 24); $textoNome.Font = $fontes.Normal
$textoNome.Add_TextChanged({ SelecionarRegistroDigitado })
$textoNome.Add_KeyDown({ if ($_.KeyCode -eq "Return") { $_.Handled = $true; $botaoAdicionar.PerformClick() } })
$painelFormulario.Controls.Add($textoNome)

$painelFormulario.Controls.Add((New-LabelEx "Ativo:" 16 78 80 20 $fontes.Bold $cores.Texto))
$comboAtivo = New-Object System.Windows.Forms.ComboBox; $comboAtivo.Location = New-Object System.Drawing.Point(100, 76); $comboAtivo.Size = New-Object System.Drawing.Size(150, 24); $comboAtivo.DropDownStyle = "DropDownList"; $comboAtivo.Font = $fontes.Normal
$comboAtivo.Items.AddRange(@("Sim", "Nao")); $comboAtivo.SelectedIndex = 0; $painelFormulario.Controls.Add($comboAtivo)

# Checkbox Visitante
$chkVisitante = New-Object System.Windows.Forms.CheckBox
$chkVisitante.Text = "Visitante"; $chkVisitante.Location = New-Object System.Drawing.Point(260, 76); $chkVisitante.Size = New-Object System.Drawing.Size(100, 24)
$chkVisitante.Font = $fontes.Normal; $chkVisitante.Checked = $false
$chkVisitante.Add_CheckedChanged({ AtualizarEstadoVisitante })
$painelFormulario.Controls.Add($chkVisitante)

$painelFormulario.Controls.Add((New-LabelEx "Idade:" 16 114 80 20 $fontes.Bold $cores.Texto))
$comboIdade = New-Object System.Windows.Forms.ComboBox; $comboIdade.Location = New-Object System.Drawing.Point(100, 112); $comboIdade.Size = New-Object System.Drawing.Size(220, 24); $comboIdade.DropDownStyle = "DropDownList"; $comboIdade.Font = $fontes.Normal
$comboIdade.Items.AddRange(@("Adulto", "Juvenil", "Jovem", "Crianca")); $comboIdade.SelectedIndex = 0; $painelFormulario.Controls.Add($comboIdade)
$paginaMembros.Controls.Add($painelFormulario)

# ====== PAINEL DE ACOES ======
$painelAcoes = New-PanelEx $cores.Branco; $painelAcoes.Location = New-Object System.Drawing.Point(648, 480); $painelAcoes.Size = New-Object System.Drawing.Size(608, 170)
$botaoAdicionar = New-ButtonEx "+ ADICIONAR" 16 16 576 44 $cores.Branco $cores.Verde { ExecutarAcaoRegistro "adicionar" }
$tooltip.SetToolTip($botaoAdicionar, "Adicionar novo membro")
$botaoEditar = New-ButtonEx "EDITAR" 16 72 276 42 $cores.Texto $cores.Amarelo { ExecutarAcaoRegistro "editar" }
$tooltip.SetToolTip($botaoEditar, "Editar membro selecionado")
$botaoApagar = New-ButtonEx "APAGAR" 316 72 276 42 $cores.Branco $cores.Verm { ExecutarAcaoRegistro "remover" }
$tooltip.SetToolTip($botaoApagar, "Remover membro selecionado")
$painelAcoes.Controls.AddRange(@($botaoAdicionar, $botaoEditar, $botaoApagar)); $paginaMembros.Controls.Add($painelAcoes)

# ====== AJUSTES DE LAYOUT ======
function AjustarLayout {
  $w = $form.ClientSize.Width; $h = $form.ClientSize.Height
  if ($topoPresenca) { $topoPresenca.Width = $w }
  if ($botaoPlanilha) { $botaoPlanilha.Left = $w - 320 }
  if ($botaoGerenciador) { $botaoGerenciador.Left = $w - 160 }
  if ($topoMembros) { $topoMembros.Width = $w }
  if ($painelInicio) { $painelInicio.Width = $w }
  if ($painelListaPresencas) { $painelListaPresencas.Width = $w; if ($botaoEditarChamada) { $botaoEditarChamada.Left = $w - 160 }; if ($tabelaChamadas) { $tabelaChamadas.Width = $w - 32 } }
  if ($painelChamada) {
    $painelChamada.Width = $w; $painelChamada.Height = [Math]::Max(300, $h - 64)
    if ($topoChamada) { $topoChamada.Width = $w; if ($botaoFinalizar) { $botaoFinalizar.Left = $w - 150 } }
    if ($listaCartoes) { $listaCartoes.Width = $w; $listaCartoes.Height = [Math]::Max(200, $h - 64 - 52) }
    if ($painelChamada.Visible -and $listaCartoes.Controls.Count -gt 0) { AtualizarCartoesPresenca }
  }
  if ($tabelaRegistros) { $tabelaRegistros.Width = $w - 24 }
  $panelAltura = 170
  if ($painelFormulario) { $painelFormulario.Width = [int]($w * 0.49) - 12; $painelFormulario.Height = $panelAltura }
  if ($painelAcoes) {
    $painelAcoes.Left = [int]($w * 0.49) + 12; $painelAcoes.Width = [int]($w * 0.49) - 24; $painelAcoes.Height = $panelAltura
    $aw = $painelAcoes.Width - 32; if ($botaoAdicionar) { $botaoAdicionar.Width = $aw }
    $bw = [Math]::Max(120, [int](($aw - 16) / 2))
    if ($botaoEditar) { $botaoEditar.Width = $bw }; if ($botaoApagar) { $botaoApagar.Left = $bw + 32; $botaoApagar.Width = $bw }
  }
}

$form.Add_Resize({ AjustarLayout }); AjustarLayout
$form.Add_FormClosing({
    if ($painelChamada -and $painelChamada.Visible -and $script:arquivoPresenca -and (ChamadaTemAlteracoes)) {
      $resposta = PerguntarSalvarAlteracoesChamada
      if ($resposta -eq [System.Windows.Forms.DialogResult]::Cancel) {
        $_.Cancel = $true
        return
      }
      if ($resposta -eq [System.Windows.Forms.DialogResult]::Yes -and -not (SalvarChamadaAtual)) {
        $_.Cancel = $true
        return
      }
    }
  })
$form.Controls.AddRange(@($paginaPresenca, $paginaMembros))
CarregarRegistros; AtualizarListaChamadas
$form.ShowDialog() | Out-Null
