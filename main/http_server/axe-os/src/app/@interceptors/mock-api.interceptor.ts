import { Injectable } from '@angular/core';
import {
  HttpEvent,
  HttpHandler,
  HttpInterceptor,
  HttpRequest,
  HttpResponse,
} from '@angular/common/http';
import { Observable, of } from 'rxjs';
import { delay } from 'rxjs/operators';
import { SystemService } from '../services/system.service';

// ─────────────────────────────────────────────────────────────────────────────
//  MOCK API INTERCEPTOR  (SOLO DESARROLLO LOCAL)
//
//  Sirve datos falsos para que la web se vea poblada sin un minero conectado,
//  ideal para retocar temas/estilos en `npm start`.
//
//  ACTIVACIÓN: solo cuando la web corre en el puerto 4200 (el `ng serve` de
//  desarrollo). El minero real sirve en el puerto 80, así que esto queda INERTE
//  aunque el fichero llegara a compilarse en el firmware. Se puede forzar con
//  localStorage 'useMocks' = '1' (on) / '0' (off).
// ─────────────────────────────────────────────────────────────────────────────

function mocksEnabled(): boolean {
  try {
    const f = localStorage.getItem('useMocks');
    if (f === '0') return false;
    if (f === '1') return true;
  } catch {
    /* localStorage no disponible: usar default */
  }
  return typeof window !== 'undefined' && window.location.port === '4200';
}

// ── Historial sintético (se genera una vez; timestamps estables → sin crecer) ──
function buildHistory() {
  const N = 121;
  const stepMs = 30_000; // 30 s entre puntos → ~1 h de ventana
  const base = Date.now() - (N - 1) * stepMs;

  const timestamps: number[] = [];
  const hashrate_1m: number[] = [];
  const hashrate_10m: number[] = [];
  const hashrate_1h: number[] = [];
  const hashrate_1d: number[] = [];
  const asicTemp: number[] = [];
  const vregTemp: number[] = [];

  for (let i = 0; i < N; i++) {
    timestamps.push(i * stepMs); // relativo a timestampBase
    const wobble = Math.sin(i / 6) * 35 + (Math.random() - 0.5) * 25;
    // El gráfico hace hashrate × 1e9 / 100  → el firmware manda GH/s × 100 (centi-GH/s)
    hashrate_1m.push(Math.round((2522 + wobble) * 100));
    hashrate_10m.push(Math.round((2500 + wobble * 0.6) * 100));
    hashrate_1h.push(Math.round((2470 + wobble * 0.3) * 100));
    hashrate_1d.push(Math.round((2440 + wobble * 0.15) * 100));
    // El gráfico hace temp / 100  → el firmware manda °C × 100 (centi-°C)
    asicTemp.push(Math.round((52 + Math.sin(i / 9) * 2) * 100));
    vregTemp.push(Math.round((57 + Math.sin(i / 9) * 2) * 100));
  }

  return {
    hashrate_1m,
    hashrate_10m,
    hashrate_1h,
    hashrate_1d,
    asicTemp,
    vregTemp,
    hasMore: false,
    timestamps,
    timestampBase: base,
  };
}

const MOCK_ASIC = {
  ASICModel: 'BM1370',
  deviceModel: 'NerdAxe',
  asicCount: 1,
  swarmColor: 'orange',
  defaultFrequency: 375,
  defaultVoltage: 1150,
  absMaxFrequency: 800,
  absMaxVoltage: 1400,
  frequencyOptions: [300, 325, 350, 375, 400, 425, 450, 490, 525, 575],
  voltageOptions: [1000, 1050, 1100, 1150, 1200, 1250, 1300],
};

const MOCK_INFLUX = {
  influxEnable: 0,
  influxURL: '',
  influxPort: 8086,
  influxToken: '',
  influxBucket: '',
  influxOrg: '',
  influxPrefix: 'nerdqaxe',
};

const MOCK_ALERT = {
  alertEnable: 0,
  alertTelegramToken: '',
  alertTelegramChatId: '',
  alertDiscordWebhook: '',
  alertOnPoolDown: 1,
  alertOnHashrateDrop: 1,
};

@Injectable()
export class MockApiInterceptor implements HttpInterceptor {
  private tick = 0;
  private readonly history = buildHistory();

  intercept(req: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {
    if (!mocksEnabled() || !req.url.includes('/api/')) {
      return next.handle(req);
    }

    const url = req.url.split('?')[0];
    const method = req.method.toUpperCase();
    let body: any = undefined;

    if (method === 'GET') {
      if (url.endsWith('/api/system/info')) body = this.buildInfo();
      else if (url.endsWith('/api/system/asic')) body = MOCK_ASIC;
      else if (url.endsWith('/api/swarm/info')) body = [];
      else if (url.endsWith('/api/influx/info')) body = MOCK_INFLUX;
      else if (url.endsWith('/api/alert/info')) body = MOCK_ALERT;
      else if (url.endsWith('/api/otp/status')) body = { enabled: false };
      else if (url.includes('/api/history/len')) body = { length: 0 };
      else if (url.includes('/api/history/data')) body = { ts: [] };
      else if (url.includes('/api/system/OTA/github')) body = { status: 'idle' };
      else if (url.endsWith('/api/v2/dashboard')) body = this.buildDashboardV2();
      else if (url.endsWith('/api/v2/identify')) body = this.buildIdentifyV2();
      else if (url.endsWith('/api/v2/system')) body = this.buildSystemV2();
    }

    // Escrituras (guardar ajustes, etc.): fingir éxito para que la UI no falle.
    if (body === undefined && (method === 'POST' || method === 'PATCH')) {
      return of(new HttpResponse({ status: 200, body: 'ok' })).pipe(delay(120));
    }

    // Endpoint no mockeado: dejar pasar (fallará sin backend, pero sin ruido).
    if (body === undefined) {
      return next.handle(req);
    }

    return of(new HttpResponse({ status: 200, body })).pipe(delay(120));
  }

  private buildInfo(): any {
    // Partimos de la forma completa real y sobreescribimos lo visible.
    const info: any = JSON.parse(JSON.stringify(SystemService.defaultInfo()));

    const t = this.tick++;
    const live = 1100 + Math.sin(t / 4) * 40 + (Math.random() - 0.5) * 30;
    const tempLive = 53 + Math.sin(t / 7) * 1.5;

    Object.assign(info, {
      power: Math.round((23.5 + Math.sin(t / 5) * 0.6) * 100) / 100,
      minPower: 14,
      maxPower: 25,
      voltage: 5020,       // mV (the app divides by 1000 -> 5.02 V)
      minVoltage: 4.5,     // already V (not divided)
      maxVoltage: 5.5,
      current: 4800,       // mA -> 4.8 A
      currentA: 4.8,
      minCurrentA: 0,
      maxCurrentA: 6,
      temp: Math.round(tempLive * 10) / 10,
      vrTemp: Math.round((57 + Math.sin(t / 7) * 1.5) * 10) / 10,
      vrTempInt: 0,
      hashRateTimestamp: Date.now(),
      hashRate: Math.round(live * 100) / 100,
      hashRate_10m: Math.round((live - 15) * 100) / 100,
      hashRate_1h: Math.round((live - 40) * 100) / 100,
      hashRate_1d: Math.round((live - 70) * 100) / 100,
      bestDiff: 1_234_567_890,
      bestSessionDiff: 45_678_901,
      coreVoltage: 1150,        // mV -> 1.15 V
      defaultCoreVoltage: 1150,
      coreVoltageActual: 1150,  // mV -> 1.15 V
      hostname: 'nerdqaxe-demo',
      hostip: '192.168.1.50',
      macAddr: 'AA:BB:CC:DD:EE:FF',
      wifiRSSI: -54,
      ssid: 'BitronicsLab',
      wifiStatus: 'Connected!',
      sharesAccepted: 12_840 + t,
      sharesRejected: 7,
      uptimeSeconds: 86_400 + t * 2,
      asicCount: 1,
      smallCoreCount: 6667,
      ASICModel: 'BM1370',
      deviceModel: 'NerdAxe',
      stratumURL: 'pool.bitronics.com',
      stratumPort: 3333,
      stratumUser: 'bc1q29hp4fqtks2wzpmfwtpac64fnr8ujw2nvnra04.nerdqaxe',
      frequency: 375,
      defaultFrequency: 375,
      version: 'demo-1.0',
      fanspeed: 82,
      fanrpm: 4920,
      lastResetReason: 'Power on',
      poolDifficulty: 512,
      stratumDifficulty: 512,
      vrFrequency: 600,
      defaultTheme: 'gaia',
    });

    info.stratum = {
      poolMode: 0,
      activePoolMode: 0,
      usingFallback: false,
      totalBestDiff: 1_234_567_890,
      pools: [
        {
          connected: true,
          poolDiffErr: false,
          poolDifficulty: 512,
          accepted: 12_840 + t,
          rejected: 7,
          bestDiff: 1_234_567_890,
          pingRtt: 63,
          pingLoss: 0,
          activeProtocol: 0,
          encrypted: false,
        },
      ],
    };

    info.history = this.history;
    return info;
  }

  // Nested IDashboardV2 shape (the endpoint the redesigned dashboard polls).
  private buildDashboardV2(): any {
    const t = this.tick++;
    const live = 2522 + Math.sin(t / 4) * 45 + (Math.random() - 0.5) * 30;
    const asicT = 53 + Math.sin(t / 7) * 1.5;
    const vrT = 57 + Math.sin(t / 7) * 1.5;
    const accepted = 1967 + t;

    const pool = {
      host: 'pool.bitronics.com',
      port: 3333,
      user: 'bc1q29hp4fqtks2wzpmfwtpac64fnr8ujw2nvnra04.nerdqaxe',
      connected: true,
      activeProtocol: 0,
      encrypted: false,
      accepted,
      rejected: 3,
      bestDiff: 3_590_000_000,
      pingRtt: 63,
      pingLoss: 0,
      poolDifficulty: 512,
      networkDifficulty: 1.21e14,
    };

    return {
      system: { uptime: 86_400 + t * 2, shutdown: false, boardError: 0, overheatTemp: 0 },
      performance: {
        hashRateTimestamp: Date.now(),
        hashRate: Math.round(live * 100) / 100,
        hashRate1m: Math.round(live * 100) / 100,
        hashRate10m: Math.round((live - 20) * 100) / 100,
        hashRate1h: Math.round((live - 50) * 100) / 100,
        hashRate1d: Math.round((live - 90) * 100) / 100,
        bestDiff: 3_590_000_000,
        bestSessionDiff: 7_280_000,
        sharesAccepted: accepted,
        sharesRejected: 3,
        frequency: 375,
        asicCount: 1,
        smallCoreCount: 6667,
      },
      power: {
        watts: Math.round((30.9 + Math.sin(t / 5) * 0.5) * 100) / 100,
        min: 5,
        max: 40,
        voltage: 12.0,
        voltageMin: 10.8,
        voltageMax: 13.2,
        currentA: Math.round((2.58 + Math.sin(t / 5) * 0.05) * 100) / 100,
        currentAMin: 0,
        currentAMax: 6,
        coreVoltageActual: 1.0,
      },
      thermal: {
        asicTemp: Math.round(asicT * 10) / 10,
        vrTemp: Math.round(vrT * 10) / 10,
        vrTempInt: 0,
        asicTemps: [Math.round(asicT * 10) / 10],
        fans: [{ speed: 82, rpm: 4920 }],
      },
      stratum: {
        poolMode: 0,
        activePoolMode: 0,
        usingFallback: false,
        totalBestDiff: 3_590_000_000,
        poolBalance: 0,
        pools: [pool],
      },
      can: { hasExtension: false, enabled: false },
      coinbase: { blockHeaders: [], pools: [] },
      history: this.history,
    };
  }

  // IIdentifyV2: device model + default theme + CAN flag (used by the header).
  private buildIdentifyV2(): any {
    return {
      deviceModel: 'NerdAxeGaia',
      defaultTheme: 'gaia',
      can: { enabled: false },
    };
  }

  // ISystemV2 — used by the System page (getSystemV2 -> /api/v2/system).
  private buildSystemV2(): any {
    return {
      deviceModel: 'NerdAxeGaia',
      asicModel: 'BM1373',
      version: 'demo-1.0',
      uptimeSeconds: 86_400,
      lastResetReason: 'Power on',
      network: {
        hostname: 'nerdaxegaia-demo',
        ssid: 'BitronicsLab',
        macAddr: 'AA:BB:CC:DD:EE:FF',
        ipAddr: '192.168.1.50',
        wifiStatus: 'Connected!',
        wifiRSSI: -54,
      },
      memory: { freeHeap: 120_000, freeHeapInt: 45_000 },
    };
  }
}
