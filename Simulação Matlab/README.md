# Modelo de Rede de Ar Comprimido (Moist Air)

Este documento descreve a função de cada trecho do **script MATLAB/Simulink** que gera automaticamente um modelo de rede de ar comprimido usando **Simscape Fluids – Moist Air (MA)**. O código está anexado no repositório, aqui fornecemos apenas a explicação para facilitar a manutenção e compreensão.


## Estrutura Geral

O script cria um novo modelo no Simulink, insere os blocos necessários (reservatórios, tubulações, curvas, sensores e junções), posiciona cada bloco na área de diagrama, e conecta as portas de acordo com a topologia desejada.

### Criação do Modelo

* Verifica se já existe um modelo com o nome especificado e, se existir, fecha-o para evitar conflitos.
* Cria um novo sistema (`new_system`) e abre a janela do modelo (`open_system`).

### Inserção dos Blocos

* **Compressor (Reservoir MA)**: representa a fonte de ar comprimido.
* **T-Junction (MA)**: ponto de divisão do fluxo em duas derivações.
* **Pipe (MA)**: trechos de tubulação com comprimento definido.
* **Local Restriction (MA)**: representa curvas ou perdas localizadas.
* **Flow Rate Sensor (MA)**: mede a vazão de ar.
* **Reservoir (MA)**: simula a atmosfera (saída do sistema).

Cada bloco recebe um nome identificador (ex.: `D1_Pipe1`) e uma posição definida por coordenadas gráficas.

### Conexões

* As conexões entre blocos usam `add_line`, garantindo que o ar comprimido flua desde o compressor, passando pela T-Junction principal, e seguindo pelas duas derivações.
* Os sensores e reservatórios são conectados ao final de cada derivação.

### Posicionamento dos Blocos

O posicionamento é definido pelo parâmetro **`Position`**, que recebe um vetor `[x1 y1 x2 y2]` representando o retângulo do bloco no diagrama Simulink:

* `x1, y1` → canto superior esquerdo.
* `x2, y2` → canto inferior direito.

Isso garante que cada bloco apareça em uma localização organizada, permitindo leitura clara do fluxo.

### Autorouting das Conexões

As linhas de conexão usam a opção **`'autorouting','on'`**. Essa configuração instrui o Simulink a **rotear automaticamente as linhas** para evitar cruzamentos desnecessários e manter o diagrama limpo, sem necessidade de ajustar manualmente cada caminho.


## Organização do Script

O código é dividido em seções para maior clareza:

1. **Inicialização** – Criação do modelo e fechamento de versões antigas.
2. **Blocos principais** – Inserção do compressor e T-Junction.
3. **Derivação 1 (D1)** – Tubulações, curvas e sensores da primeira ramificação.
4. **Derivação 2 (D2)** – Tubulações, curvas e sensores da segunda ramificação.
5. **Conexões principais** – Interligações entre todos os componentes, seguindo a lógica do sistema.
6. **Salvar modelo** – Opcional para registrar o arquivo `.slx` gerado.


## Observações Importantes

* Todas as bibliotecas utilizadas pertencem ao domínio **Moist Air (MA)** do Simscape Fluids.
* O uso de handles (`PortHandles`) garante que as conexões sejam feitas pelas portas corretas (entrada e saída) de cada bloco.
* A lógica de repetição (`for`) simplifica a criação de ramificações múltiplas, evitando código duplicado.
