using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Globalization;
using System.Windows.Forms.DataVisualization.Charting;

namespace Proyecto_final_Mk3
{
    public partial class Form1 : Form
    {
        SerialPort serialPortObject = new SerialPort();

        // Límite de puntos visibles en la gráfica (ventana deslizante)
        private const int MAX_POINTS = 50;

        private List<double> temperaturas = new List<double>();
        private List<double> humedades    = new List<double>();
        private List<string> timestamps   = new List<string>();

        public Form1()
        {
            InitializeComponent();
            try
            {
                timer1.Interval = 1000;
                timer1.Tick    += timer1_Tick;
                timer1.Start();

                comboBoxBaudRate.Items.Add("9600");
                comboBoxBaudRate.Items.Add("115200");
                comboBoxBaudRate.SelectedIndex = 0;

                ConfigurarGrafica();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error al inicializar: " + ex.Message);
            }
        }

        // ─────────────────────────────────────────────────────────────────
        // CONFIGURACIÓN DE LA GRÁFICA
        // ─────────────────────────────────────────────────────────────────
        private void ConfigurarGrafica()
        {
            chart1.Series.Clear();
            chart1.ChartAreas.Clear();
            chart1.Legends.Clear();

            ChartArea area = new ChartArea("ChartArea1");
            area.BackColor = Color.FromArgb(30, 34, 45);

            area.AxisX.LabelStyle.ForeColor = Color.White;
            area.AxisX.LabelStyle.Font      = new Font("Segoe UI", 7f);
            area.AxisX.LineColor            = Color.Gray;
            area.AxisX.MajorGrid.LineColor  = Color.FromArgb(60, 60, 80);
            area.AxisX.Title                = "Lecturas";
            area.AxisX.TitleForeColor       = Color.LightGray;

            area.AxisY.LabelStyle.ForeColor = Color.White;
            area.AxisY.LineColor            = Color.Gray;
            area.AxisY.MajorGrid.LineColor  = Color.FromArgb(60, 60, 80);
            area.AxisY.Title                = "Valor";
            area.AxisY.TitleForeColor       = Color.LightGray;
            area.AxisY.Minimum              = 0;
            area.AxisY.Maximum              = 100;

            chart1.ChartAreas.Add(area);

            Legend leyenda = new Legend("Legend1");
            leyenda.BackColor = Color.Transparent;
            leyenda.ForeColor = Color.White;
            leyenda.Font      = new Font("Segoe UI", 8f);
            chart1.Legends.Add(leyenda);

            // Serie Temperatura
            Series sTemp = new Series("Temperatura °C");
            sTemp.ChartType    = SeriesChartType.Line;
            sTemp.Color        = Color.OrangeRed;
            sTemp.BorderWidth  = 2;
            sTemp.ChartArea    = "ChartArea1";
            sTemp.Legend       = "Legend1";
            sTemp.MarkerStyle  = MarkerStyle.Circle;
            sTemp.MarkerSize   = 4;
            chart1.Series.Add(sTemp);

            // Serie Humedad
            Series sHum = new Series("Humedad %");
            sHum.ChartType   = SeriesChartType.Line;
            sHum.Color       = Color.DeepSkyBlue;
            sHum.BorderWidth = 2;
            sHum.ChartArea   = "ChartArea1";
            sHum.Legend      = "Legend1";
            sHum.MarkerStyle = MarkerStyle.Circle;
            sHum.MarkerSize  = 4;
            chart1.Series.Add(sHum);
        }

        // ─────────────────────────────────────────────────────────────────
        // RELOJ DIGITAL
        // ─────────────────────────────────────────────────────────────────
        private void timer1_Tick(object sender, EventArgs e)
        {
            labelClockRealtime.Text = DateTime.Now.ToString("hh:mm:ss tt");
        }

        // ─────────────────────────────────────────────────────────────────
        // BOTONES
        // ─────────────────────────────────────────────────────────────────
        private void pictureBox1_Click(object sender, EventArgs e)
        {
            this.WindowState = FormWindowState.Minimized;
        }

        private void pictureBox2_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void buttonRefresh_Click(object sender, EventArgs e)
        {
            string[] ports = SerialPort.GetPortNames();
            comboBoxPortSelection.Items.Clear();
            comboBoxPortSelection.Items.AddRange(ports);
        }

        private void buttonConnect_Click(object sender, EventArgs e)
        {
            try
            {
                if (!serialPortObject.IsOpen)
                {
                    OpenSerialPort();
                    buttonConnect.Text      = "Disconnect";
                    buttonConnect.BackColor = Color.Red;
                    serialPortObject.DataReceived += port_DataReceived;
                }
                else
                {
                    serialPortObject.DataReceived -= port_DataReceived;
                    CloseSerialPort();
                    buttonConnect.Text      = "Connect";
                    buttonConnect.BackColor = Color.Green;
                    ResetAppToDefaultState();
                }
            }
            catch (Exception error)
            {
                MessageBox.Show(error.Message);
                buttonConnect.Text      = "Connect";
                buttonConnect.BackColor = Color.Green;
            }
        }

        private void OpenSerialPort()
        {
            if (comboBoxBaudRate.Text == "" || comboBoxPortSelection.Text == "")
            {
                MessageBox.Show("Selecciona puerto y baud rate");
                return;
            }
            serialPortObject.BaudRate = int.Parse(comboBoxBaudRate.Text);
            serialPortObject.PortName = comboBoxPortSelection.Text;
            serialPortObject.Open();
        }

        private void CloseSerialPort()
        {
            if (serialPortObject.IsOpen)
                serialPortObject.Close();
        }

        // ─────────────────────────────────────────────────────────────────
        // RECEPCIÓN DE DATOS DEL DHT11
        //
        // El Arduino debe enviar líneas con este formato exacto:
        //     T:25.00,H:60.00
        //
        // Código Arduino de ejemplo:
        //   #include <DHT.h>
        //   #define DHTPIN 2
        //   #define DHTTYPE DHT11
        //   DHT dht(DHTPIN, DHTTYPE);
        //   void setup() { Serial.begin(9600); dht.begin(); }
        //   void loop() {
        //     float t = dht.readTemperature();
        //     float h = dht.readHumidity();
        //     if (!isnan(t) && !isnan(h)) {
        //       Serial.print("T:"); Serial.print(t);
        //       Serial.print(",H:"); Serial.println(h);
        //     }
        //     delay(2000);
        //   }
        // ─────────────────────────────────────────────────────────────────
        private void port_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                string line = serialPortObject.ReadLine().Trim();
                this.Invoke((MethodInvoker)delegate { ProcessDataFromArduino(line); });
            }
            catch { /* ignorar errores de lectura parcial */ }
        }

        private void ProcessDataFromArduino(string line)
        {
            try
            {
                double temperatura = double.NaN;
                double humedad     = double.NaN;

                foreach (string parte in line.Split(','))
                {
                    string[] kv = parte.Split(':');
                    if (kv.Length != 2) continue;

                    string clave = kv[0].Trim().ToUpper();
                    double valor;
                    if (!double.TryParse(kv[1].Trim(), NumberStyles.Float,
                                         CultureInfo.InvariantCulture, out valor)) continue;

                    if      (clave == "T") temperatura = valor;
                    else if (clave == "H") humedad     = valor;
                }

                if (!double.IsNaN(temperatura) && !double.IsNaN(humedad))
                {
                    // Actualizar etiquetas
                    labelTemperature.Text = $"Temperatura: {temperatura:F1} °C";
                    labelHumidity.Text    = $"Humedad: {humedad:F1} %";

                    // Agregar a listas con ventana deslizante
                    temperaturas.Add(temperatura);
                    humedades.Add(humedad);
                    timestamps.Add(DateTime.Now.ToString("HH:mm:ss"));

                    if (temperaturas.Count > MAX_POINTS)
                    {
                        temperaturas.RemoveAt(0);
                        humedades.RemoveAt(0);
                        timestamps.RemoveAt(0);
                    }

                    ActualizarGrafica();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error procesando datos: " + ex.Message);
            }
        }

        private void ActualizarGrafica()
        {
            chart1.Series["Temperatura °C"].Points.Clear();
            chart1.Series["Humedad %"].Points.Clear();

            for (int i = 0; i < temperaturas.Count; i++)
            {
                chart1.Series["Temperatura °C"].Points.AddXY(i + 1, temperaturas[i]);
                chart1.Series["Humedad %"].Points.AddXY(i + 1, humedades[i]);
            }

            if (temperaturas.Count > 0)
            {
                chart1.ChartAreas["ChartArea1"].AxisX.Minimum = 1;
                chart1.ChartAreas["ChartArea1"].AxisX.Maximum = temperaturas.Count;
            }

            chart1.Invalidate();
        }

        private void ResetAppToDefaultState()
        {
            temperaturas.Clear();
            humedades.Clear();
            timestamps.Clear();

            chart1.Series["Temperatura °C"].Points.Clear();
            chart1.Series["Humedad %"].Points.Clear();

            labelTemperature.Text = "Temperatura °C";
            labelHumidity.Text    = "Humedad";

            chart1.Invalidate();
        }

        // ─────────────────────────────────────────────────────────────────
        // PAINT HANDLERS
        // ─────────────────────────────────────────────────────────────────
        private void panel2_Paint(object sender, PaintEventArgs e)  { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel3_Paint(object sender, PaintEventArgs e)  { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel6_Paint(object sender, PaintEventArgs e)  { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel7_Paint(object sender, PaintEventArgs e)  { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel8_Paint(object sender, PaintEventArgs e)  { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel9_Paint(object sender, PaintEventArgs e)  { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel10_Paint(object sender, PaintEventArgs e) { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel11_Paint(object sender, PaintEventArgs e) { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }
        private void panel12_Paint(object sender, PaintEventArgs e) { e.Graphics.Clear(Color.FromArgb(30, 34, 45)); }

        // ─────────────────────────────────────────────────────────────────
        // EVENTOS VACÍOS (requeridos por el diseñador)
        // ─────────────────────────────────────────────────────────────────
        private void label1_Click(object sender, EventArgs e)                  { }
        private void labelClockRealtime_Click(object sender, EventArgs e)      { }
        private void Form1_Load_1(object sender, EventArgs e)                  { }
        private void comboBoxBaudRate_SelectedIndexChanged(object sender, EventArgs e) { }
        private void comboBoxPortSelection_SelectedIndexChanged(object sender, EventArgs e) { }
        private void pictureBox4_Click(object sender, EventArgs e)             { }
        private void chart1_Click(object sender, EventArgs e)                  { }
    }
}
