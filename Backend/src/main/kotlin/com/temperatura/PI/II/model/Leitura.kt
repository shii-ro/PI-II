package com.temperatura.PI.II.model


import jakarta.persistence.*
import java.time.LocalDateTime

@Entity
data class Leitura(
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    val id: Long = 0,

    val temperatura: Double,

    val umidade: Double? = null,

    // Identifica de qual dispositivo/ESP32 veio a leitura (util quando houver mais de um sensor)
    val dispositivo: String = "esp32-01",

    val dataHora: LocalDateTime = LocalDateTime.now()
)
