package com.temperatura.PI.II.service

import com.temperatura.PI.II.dto.EstatisticasResponse
import com.temperatura.PI.II.dto.LeituraRequest
import com.temperatura.PI.II.dto.LeituraResponse
import com.temperatura.PI.II.dto.toEntity
import com.temperatura.PI.II.dto.toResponse
import com.temperatura.PI.II.repository.LeituraRepository
import org.springframework.stereotype.Service
import java.time.LocalDateTime

@Service
class LeituraService(
    private val repository: LeituraRepository
) {

    fun salvar(request: LeituraRequest): LeituraResponse =
        repository.save(request.toEntity()).toResponse()

    fun listarTodas(): List<LeituraResponse> =
        repository.findAllByOrderByDataHoraDesc().map { it.toResponse() }

    fun listarPorPeriodo(inicio: LocalDateTime, fim: LocalDateTime): List<LeituraResponse> =
        repository.findByDataHoraBetweenOrderByDataHoraAsc(inicio, fim).map { it.toResponse() }

    fun buscarPorId(id: Long): LeituraResponse? =
        repository.findById(id).orElse(null)?.toResponse()

    fun deletar(id: Long) {
        repository.deleteById(id)
    }

    fun estatisticas(): EstatisticasResponse {
        val todas = repository.findAll()
        val ultima = repository.findTopByOrderByDataHoraDesc()

        if (todas.isEmpty()) {
            return EstatisticasResponse(
                totalLeituras = 0,
                temperaturaAtual = null,
                temperaturaMedia = null,
                temperaturaMinima = null,
                temperaturaMaxima = null,
                ultimaLeituraEm = null
            )
        }

        val temperaturas = todas.map { it.temperatura }

        return EstatisticasResponse(
            totalLeituras = todas.size.toLong(),
            temperaturaAtual = ultima?.temperatura,
            temperaturaMedia = temperaturas.average(),
            temperaturaMinima = temperaturas.min(),
            temperaturaMaxima = temperaturas.max(),
            ultimaLeituraEm = ultima?.dataHora
        )
    }
}
