%% ================================
% Script de Simulação da Rede de Ar Comprimido (MA)
% - Variação da pressão de entrada
% - Pressões atmosféricas aleatórias nos Reservoirs de saída
% - Coleta de médias de vazão e pressão
% ================================

clearvars; close all;

% --- Nome do modelo
modelName = 'Modelo_Rede_Ar_Comprimido_MA';

% --- Carrega o modelo na memória (sem abrir janela)
load_system(modelName);

% --- Ativa Simscape Results Logging
set_param(modelName,'SimscapeLogType','all');

%% --- Intervalos de teste de pressão de entrada (2 a 6 bar em Pa)
P_in_range = linspace(2e5, 6e5, 5); % [Pa]
N_runs     = numel(P_in_range);

%% --- Sensores de vazão e pressão (nomes conforme blocos do modelo)
flowSensors = ["D1_Out1","D1_Out2","D1_Out3","D1_Out4","D2_Out1","D2_Out2"];
pressureSensors = flowSensors; % se sensores de pressão distintos, altere

%% --- Lista de Reservoirs de saída (nomes exatos no modelo)
atmBlocks = ["D1_Atmos_1","D1_Atmos_2","D1_Atmos_3","D1_Atmos_4","D2_Atmos_1","D2_Atmos_2"];

% --- Verificação de consistência
if numel(atmBlocks) ~= numel(flowSensors)
    warning('Número de atmBlocks e flowSensors difere. Verifique nomes e ajuste o script.');
end

%% --- Tabela para armazenar resultados
results = table();

%% --- Parâmetros de pressões atmosféricas aleatórias (MPa)
atm_min_MPa = 0.100;
atm_max_MPa = 0.120;

%% --- Loop de simulação
for k = 1:N_runs
    fprintf('Run %d / %d — P_in = %.0f Pa (%.3f MPa)\n', k, N_runs, P_in_range(k), P_in_range(k)/1e6);

    %% 1) Pressão de entrada (Compressor / Reservoir)
    P_in = P_in_range(k);       % [Pa]
    P_in_MPa = P_in / 1e6;      % [MPa]

    compressorPath = sprintf('%s/%s', modelName, 'Compressor');

    try
        % Ajuste apenas do valor da pressão
        set_param(compressorPath,'reservoir_pressure', num2str(P_in_MPa));
    catch ME
        warning('Erro ao ajustar compressor (%s): %s', compressorPath, ME.message);
    end

    %% 2) Pressões aleatórias nos Reservoirs de saída
    for j = 1:numel(atmBlocks)
        blk = sprintf('%s/%s', modelName, atmBlocks(j));
        P_res_MPa = atm_min_MPa + (atm_max_MPa - atm_min_MPa) * rand();
        try
            set_param(blk,'reservoir_pressure', num2str(P_res_MPa));
        catch ME
            warning('Erro ao ajustar %s: %s', blk, ME.message);
        end
    end

    %% 3) Executar simulação
    StopTime = 10; % [s] tempo suficiente para atingir regime
    simOut = sim(modelName, 'StopTime', num2str(StopTime), 'ReturnWorkspaceOutputs','on');

    %% 4) Acessar Simscape log
    if isprop(simOut,'simlog')
        log = simOut.simlog;
    else
        try
            log = simscape.logging.getSimulationLog(simOut);
        catch
            error('Não foi possível obter simlog. Verifique SimscapeLogType.');
        end
    end

    %% 5) Extrair resultados médios
    row = table(P_in, 'VariableNames', {'Pressao_Entrada_Pa'});

    for s = 1:numel(flowSensors)
        sensorName = flowSensors(s);
        valueFlow  = NaN;
        valuePress = NaN;

        % --- Vazão
        try
            q_series = eval(['log.' sensorName '.q.series.values;']);
            valueFlow = mean(q_series(:));
        catch
            try
                slog = simlog2struct(log);
                if isfield(slog, char(sensorName)) && isfield(slog.(char(sensorName)),'q')
                    valueFlow = mean(slog.(char(sensorName)).q.series.values(:));
                end
            catch
                warning('Não foi possível extrair vazão do sensor "%s".', sensorName);
            end
        end

        % --- Pressão
        try
            p_series = eval(['log.' sensorName '.p.series.values;']);
            valuePress = mean(p_series(:));
        catch
            try
                if exist('slog','var') && isfield(slog, char(sensorName)) && isfield(slog.(char(sensorName)),'p')
                    valuePress = mean(slog.(char(sensorName)).p.series.values(:));
                end
            catch
                % mantém NaN
            end
        end

        % --- Adiciona ao row
        colFlowName  = matlab.lang.makeValidName(sprintf('Vazao_%s', sensorName));
        colPressName = matlab.lang.makeValidName(sprintf('Pressao_%s_Pa', sensorName));
        row.(colFlowName)  = valueFlow;
        row.(colPressName) = valuePress;
    end

    % --- Concatena à tabela de resultados
    results = [results; row];

    % --- Limpeza opcional
    clear simOut log;
end

%% --- Exibe e salva resultados
disp(results);
writetable(results, 'resultados_rede_ar.csv');

%% ================================
% Helper: converte simlog para struct
%% ================================
function s = simlog2struct(logObj)
    s = struct();
    fn = fieldnames(logObj);
    for i=1:numel(fn)
        name = fn{i};
        val  = logObj.(name);
        try
            subfn = fieldnames(val);
            s.(name) = simlog2struct(val);
        catch
            s.(name) = val;
        end
    end
end
