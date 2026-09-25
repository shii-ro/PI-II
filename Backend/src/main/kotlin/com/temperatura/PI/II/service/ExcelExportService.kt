package com.temperatura.PI.II.service

import com.temperatura.PI.II.dto.LeituraResponse
import org.apache.poi.ss.usermodel.FillPatternType
import org.apache.poi.ss.usermodel.IndexedColors
import org.apache.poi.xssf.usermodel.XSSFWorkbook
import org.springframework.stereotype.Service
import java.io.ByteArrayOutputStream
import java.time.format.DateTimeFormatter

@Service
class ExcelExportService {

    private val formatter = DateTimeFormatter.ofPattern("dd/MM/yyyy HH:mm:ss")

    fun gerarExcel(leituras: List<LeituraResponse>): ByteArray {
        XSSFWorkbook().use { workbook ->
            val sheet = workbook.createSheet("Leituras")

            val headerStyle = workbook.createCellStyle().apply {
                fillForegroundColor = IndexedColors.GREY_25_PERCENT.index
                fillPattern = FillPatternType.SOLID_FOREGROUND
            }

            val colunas = listOf("ID", "Dispositivo", "Temperatura (°C)", "Umidade (%)", "Data/Hora")
            val header = sheet.createRow(0)
            colunas.forEachIndexed { i, titulo ->
                val cell = header.createCell(i)
                cell.setCellValue(titulo)
                cell.cellStyle = headerStyle
            }

            leituras.forEachIndexed { index, leitura ->
                val row = sheet.createRow(index + 1)
                row.createCell(0).setCellValue(leitura.id.toDouble())
                row.createCell(1).setCellValue(leitura.dispositivo)
                row.createCell(2).setCellValue(leitura.temperatura)
                leitura.umidade?.let { row.createCell(3).setCellValue(it) }
                row.createCell(4).setCellValue(leitura.dataHora.format(formatter))
            }

            for (i in colunas.indices) sheet.autoSizeColumn(i)

            val out = ByteArrayOutputStream()
            workbook.write(out)
            return out.toByteArray()
        }
    }
}
