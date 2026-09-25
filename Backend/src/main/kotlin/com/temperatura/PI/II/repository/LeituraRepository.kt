package com.temperatura.PI.II.repository


import com.temperatura.PI.II.model.Leitura
import org.springframework.data.jpa.repository.JpaRepository
import java.time.LocalDateTime

interface LeituraRepository : JpaRepository<Leitura, Long> {

    fun findTopByOrderByDataHoraDesc(): Leitura?

    fun findByDataHoraBetweenOrderByDataHoraAsc(
        inicio: LocalDateTime,
        fim: LocalDateTime
    ): List<Leitura>

    fun findByDispositivoOrderByDataHoraDesc(dispositivo: String): List<Leitura>

    fun findAllByOrderByDataHoraDesc(): List<Leitura>
}