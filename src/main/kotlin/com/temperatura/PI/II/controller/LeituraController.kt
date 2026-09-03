package com.temperatura.PI.II.controller




import com.temperatura.PI.II.dto.EstatisticasResponse
import com.temperatura.PI.II.dto.LeituraRequest
import com.temperatura.PI.II.dto.LeituraResponse
import com.temperatura.PI.II.service.ExcelExportService
import com.temperatura.PI.II.service.LeituraService
import jakarta.validation.Valid
import org.springframework.format.annotation.DateTimeFormat
import org.springframework.http.HttpHeaders
import org.springframework.http.HttpStatus
import org.springframework.http.MediaType
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.*
import java.time.LocalDateTime

@RestController
@RequestMapping("/api/leituras")
class LeituraController(
    private val service: LeituraService,
    private val excelExportService: ExcelExportService
) {

    // Endpoint que o ESP32 chama a cada X segundos
    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    fun receberLeitura(@Valid @RequestBody request: LeituraRequest): LeituraResponse =
        service.salvar(request)

    // Lista geral - usada pelo front-end e pelo app mobile
    @GetMapping
    fun listar(
        @RequestParam(required = false) @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) inicio: LocalDateTime?,
        @RequestParam(required = false) @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) fim: LocalDateTime?
    ): List<LeituraResponse> =
        if (inicio != null && fim != null) {
            service.listarPorPeriodo(inicio, fim)
        } else {
            service.listarTodas()
        }

    @GetMapping("/{id}")
    fun buscarPorId(@PathVariable id: Long): ResponseEntity<LeituraResponse> =
        service.buscarPorId(id)?.let { ResponseEntity.ok(it) }
            ?: ResponseEntity.notFound().build()

    @DeleteMapping("/{id}")
    @ResponseStatus(HttpStatus.NO_CONTENT)
    fun deletar(@PathVariable id: Long) = service.deletar(id)

    // Resumo para dashboards (web e mobile)
    @GetMapping("/estatisticas")
    fun estatisticas(): EstatisticasResponse = service.estatisticas()

    // Exportação para Excel
    @GetMapping("/exportar")
    fun exportarExcel(): ResponseEntity<ByteArray> {
        val leituras = service.listarTodas()
        val excelBytes = excelExportService.gerarExcel(leituras)

        val headers = HttpHeaders()
        headers.contentType = MediaType.parseMediaType(
            "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
        )
        headers.setContentDispositionFormData("attachment", "leituras.xlsx")

        return ResponseEntity(excelBytes, headers, HttpStatus.OK)
    }
}
