package com.temperatura.PI.II.dto


import com.temperatura.PI.II.model.Leitura
import jakarta.validation.constraints.NotBlank
import org.jetbrains.annotations.NotNull
import java.time.LocalDateTime

/**
 * O que o ESP32 envia via POST.
 * Mantido separado da entidade para o firmware nao precisar mandar id/dataHora.
 */
data class LeituraRequest(
    @field:NotNull
    val temperatura: Double,

    val umidade: Double? = null,

    @field:NotBlank
    val dispositivo: String = "esp32-01"
)

/**
 * O que a API devolve para os clientes (web/mobile).
 */
data class LeituraResponse(
    val id: Long,
    val temperatura: Double,
    val umidade: Double?,
    val dispositivo: String,
    val dataHora: LocalDateTime
)

fun Leitura.toResponse() = LeituraResponse(
    id = id,
    temperatura = temperatura,
    umidade = umidade,
    dispositivo = dispositivo,
    dataHora = dataHora
)

fun LeituraRequest.toEntity() = Leitura(
    temperatura = temperatura,
    umidade = umidade,
    dispositivo = dispositivo
)

/**
 * Resumo estatistico, util para dashboards (front-end web e app mobile).
 */
data class EstatisticasResponse(
    val totalLeituras: Long,
    val temperaturaAtual: Double?,
    val temperaturaMedia: Double?,
    val temperaturaMinima: Double?,
    val temperaturaMaxima: Double?,
    val ultimaLeituraEm: LocalDateTime?
)