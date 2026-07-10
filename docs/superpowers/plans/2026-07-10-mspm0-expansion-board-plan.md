# MSPM0G3507 Expansion Board Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete the schematic for the MSPM0G3507 expansion board, adding all missing connectors, power management, and wiring between existing components.

**Architecture:** Expansion board plugs into MSPM0 core board via P1/P2 female headers. Hosts 2x ICM42688 (R48/R49), power switch+LED, UART servo, I2C OLED, and two 2x6Pin TB6612 connectors. Main 3.3V from TB6612 J4.

**Tech Stack:** EasyEDA Pro + Bridge API

**Spec:** `docs/superpowers/specs/2026-07-10-mspm0-expansion-board-design.md`

## Global Constraints

- Schematic unit: 0.01inch
- **SW1 MUST be latching push-button** (self-locking, NOT momentary)
- 10V servo trace width >=1.5mm (PCB phase; annotate in schematic)
- All GND share same net "GND"
- ICM42688 INT/FSYNC/CLKIN pins = No Connect
- C18 exist at (615,670) and (620,540) — verify near R48/R49

---

## Task Plan (13 tasks, 65 steps)

### Task 1: Audit all existing components and pins

**Purpose:** Identify every component, pin, and net before placing anything new.

- [ ] **Step 1: Full component list**

```javascript
const comps = await eda.sch_PrimitiveComponent.getAll();
return comps.map(c => ({
  id: c.id, designator: c.designator||'', name: c.name||'', x: c.x, y: c.y, rotation: c.rotation||0
}));
```
Expected: 15 components with IDs. Save all IDs for subsequent tasks.

- [ ] **Step 2: Get R48/R49 pin details**

```javascript
const r48Pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<R48_ID>");
const r49Pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<R49_ID>");
return {
  r48: r48Pins?.map(p => ({number:p.number, name:p.name, net:p.net, x:p.x, y:p.y, id:p.id})),
  r49: r49Pins?.map(p => ({number:p.number, name:p.name, net:p.net, x:p.x, y:p.y, id:p.id}))
};
```
Expected: Pin lists with renamed SPI_ICM0_*/SPI_ICM1_* names confirmed.

- [ ] **Step 3: Get P1/P2 pin details**

```javascript
const p1Pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<P1_ID>");
const p2Pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<P2_ID>");
return {
  p1: p1Pins?.map(p => ({number:p.number, name:p.name, net:p.net, x:p.x, y:p.y, id:p.id})),
  p2: p2Pins?.map(p => ({number:p.number, name:p.name, net:p.net, x:p.x, y:p.y, id:p.id}))
};
```
Expected: Document pin numbering scheme. Map physical positions (H1-1..H1-17, H2-2..H2-17) to pin numbers.

- [ ] **Step 4: Get Servo/ServoPWR/unnamed component details**

```javascript
// Use IDs from Step 1 for Servo, ServoPWR, and components at (980,660) and (985,530)
const ids = ["<Servo_ID>", "<ServoPWR_ID>", "<UNNAMED_980_660_ID>", "<UNNAMED_985_530_ID>"];
const results = {};
for (const id of ids) {
  results[id] = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId(id);
}
return results;
```
Expected: Identify what unnamed components are. Identify servo/ServoPWR pins.

- [ ] **Step 5: List existing wires and nets**

```javascript
const wires = await eda.sch_PrimitiveWire.getAll();
return { wireCount: wires.length, nets: [...new Set(wires.map(w=>w.net).filter(Boolean))] };
```

- [ ] **Step 6: Commit**

```bash
git add . && git commit -m "task1: audit existing schematic components and pins"
```

---

### Task 2: Place power management components

**Purpose:** Add SW1 (latching switch), LED1 (red 0805), R_LED (1k 0603), C_BULK (10uF 0805).

- [ ] **Step 1: Search and place SW1 (self-locking switch)**

```javascript
const r = await eda.lib_Device.search("6x6 self-lock push button switch");
if (!r.length) r = await eda.lib_Device.search("自锁按键开关 6x6");
const sw1 = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:r[0].libraryUuid, uuid:r[0].uuid}, 350, 200, "", 0, false, true, true);
return {id:sw1?.id, designator:sw1?.designator};
```

- [ ] **Step 2: Place LED1 (red 0805)**

```javascript
const r = await eda.lib_Device.search("LED red 0805");
const led1 = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:r[0].libraryUuid, uuid:r[0].uuid}, 480, 200, "", 0, false, true, true);
return {id:led1?.id, designator:led1?.designator};
```

- [ ] **Step 3: Place R_LED (1k 0603)**

```javascript
const r = await eda.lib_Device.search("resistor 1K 0603");
const rLed = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:r[0].libraryUuid, uuid:r[0].uuid}, 420, 200, "", 0, false, true, true);
return {id:rLed?.id, designator:rLed?.designator};
```

- [ ] **Step 4: Place C_BULK (10uF 0805)**

```javascript
const r = await eda.lib_Device.search("capacitor 10uF 0805");
const cBulk = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:r[0].libraryUuid, uuid:r[0].uuid}, 300, 150, "", 0, false, true, true);
return {id:cBulk?.id, designator:cBulk?.designator};
```

- [ ] **Step 5: Verify all placed**

```javascript
const comps = await eda.sch_PrimitiveComponent.getAll();
const names = comps.map(c=>c.designator).filter(Boolean);
const found = names.filter(n=>['SW1','LED1','R_LED'].some(s=>n.toUpperCase().includes(s)));
return {count:found.length, found};
```

- [ ] **Step 6: Commit**

```bash
git add . && git commit -m "task2: place SW1, LED1, R_LED, C_BULK"
```

---

### Task 3: Wire main 3.3V power path

**Purpose:** Create power nets, place GND symbols, wire the power path: +3V3_IN → SW1 → +3V3_EXT → (R_LED→LED1) + C_BULK.

**Depends on:** Task 2 (component IDs)

- [ ] **Step 1: Place net ports for power nets**

```javascript
const netIn = await eda.sch_PrimitiveComponent.createNetPort('IN', '+3V3_IN', 330, 180);
const netExt = await eda.sch_PrimitiveComponent.createNetPort('OUT', '+3V3_EXT', 370, 180);
return {in:netIn?.id, ext:netExt?.id};
```

- [ ] **Step 2: Place GND symbols**

```javascript
const g1 = await eda.sch_PrimitiveComponent.createNetFlag('Ground','GND',300,120);
const g2 = await eda.sch_PrimitiveComponent.createNetFlag('Ground','GND',480,230);
const g3 = await eda.sch_PrimitiveComponent.createNetFlag('Ground','GND',300,180);
return [g1?.id,g2?.id,g3?.id];
```

- [ ] **Step 3: Get all pin positions**

```javascript
const sw=await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<SW1_ID>");
const led=await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<LED1_ID>");
const rl=await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<R_LED_ID>");
const cb=await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<C_BULK_ID>");
return {sw:sw?.map(p=>({n:p.number,x:p.x,y:p.y})), led:led?.map(p=>({n:p.number,x:p.x,y:p.y})),
  rl:rl?.map(p=>({n:p.number,x:p.x,y:p.y})), cb:cb?.map(p=>({n:p.number,x:p.x,y:p.y}))};
```

- [ ] **Step 4: Wire +3V3_IN to SW1, SW1 output to +3V3_EXT**

Using coordinates from Step 3. Wire SW1 in-series on the power path.

- [ ] **Step 5: Wire +3V3_EXT to R_LED→LED1→GND**

```javascript
// R_LED between +3V3_EXT and LED1 anode
// LED1 cathode to GND
```

- [ ] **Step 6: Wire C_BULK from +3V3_EXT to GND**

- [ ] **Step 7: Verify power nets**

```javascript
const wires = await eda.sch_PrimitiveWire.getAll();
const pw = wires.filter(w=>['+3V3_IN','+3V3_EXT','GND'].includes(w.net));
return {+3V3_IN:pw.filter(w=>w.net==='+3V3_IN').length, +3V3_EXT:pw.filter(w=>w.net==='+3V3_EXT').length};
```
Expected: >=1 wire on each power net.

- [ ] **Step 8: Commit**

```bash
git add . && git commit -m "task3: wire 3.3V power path"
```

---

### Task 4: Place TB6612 connectors (J_TB6612_J4, J_TB6612_J6)

**Purpose:** Add two 2x6Pin 2.54mm female headers.

- [ ] **Step 1: Search 2x6P header**

```javascript
const r = await eda.lib_Device.search("2x6 pin header female 2.54mm");
return r.slice(0,3).map(x=>({uuid:x.uuid,lib:x.libraryUuid,name:x.name}));
```

- [ ] **Step 2: Place J4 at (200,350)**

```javascript
const j4 = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:"<LIB>", uuid:"<UUID>"}, 200, 350, "", 0, false, true, true);
return {id:j4?.id, designator:j4?.designator};
```

- [ ] **Step 3: Place J6 at (450,350)**

```javascript
const j6 = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:"<LIB>", uuid:"<UUID>"}, 450, 350, "", 0, false, true, true);
return {id:j6?.id, designator:j6?.designator};
```

- [ ] **Step 4: Document J4/J6 pin numbering**

```javascript
const j4p = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<J4_ID>");
const j6p = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<J6_ID>");
return {
  j4: j4p?.map(p=>({n:p.number,x:p.x,y:p.y})).sort((a,b)=>a.n-b.n),
  j6: j6p?.map(p=>({n:p.number,x:p.x,y:p.y})).sort((a,b)=>a.n-b.n)
};
```
Expected: Pin numbering scheme documented for wiring.

- [ ] **Step 5: Commit**

```bash
git add . && git commit -m "task4: place J_TB6612_J4 and J_TB6612_J6"
```

---

### Task 5: Place J_I2C OLED connector

**Purpose:** Add 1x4Pin 2.54mm header for OLED.

- [ ] **Step 1: Place at (650,250)**

```javascript
const r = await eda.lib_Device.search("1x4 pin header 2.54mm");
const ji = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:r[0].libraryUuid, uuid:r[0].uuid}, 650, 250, "", 0, false, true, true);
return {id:ji?.id, designator:ji?.designator};
```

- [ ] **Step 2: Document pin order (top to bottom = by Y coordinate)**

```javascript
const pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<J_I2C_ID>");
return pins?.map(p=>({n:p.number,x:p.x,y:p.y})).sort((a,b)=>a.y-b.y);
```

- [ ] **Step 3: Commit**

```bash
git add . && git commit -m "task5: place J_I2C OLED connector"
```

---

### Task 6: Place I2C pull-up resistors

**Purpose:** R_SDA and R_SCL (4.7k 0603) from 3.3V_EXT to SDA/SCL.

- [ ] **Step 1: Place R_SDA at (600,220)**

```javascript
const r = await eda.lib_Device.search("resistor 4.7K 0603");
const rsda = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:r[0].libraryUuid, uuid:r[0].uuid}, 600, 220, "", 0, false, true, true);
return {id:rsda?.id};
```

- [ ] **Step 2: Place R_SCL at (600,190)**

```javascript
const rscl = await eda.sch_PrimitiveComponent.create(
  {libraryUuid:r[0].libraryUuid, uuid:r[0].uuid}, 600, 190, "", 0, false, true, true);
return {id:rscl?.id};
```

- [ ] **Step 3: Commit**

```bash
git add . && git commit -m "task6: place I2C pull-up resistors"
```

---

### Task 7: Wire ICM42688 SPI connections

**Purpose:** Wire R48/R49 sharing SPI1 bus with individual CS.

**Depends on:** Task 1 (R48/R49 pin IDs)

- [ ] **Step 1: Place SPI net ports**

```javascript
const sclk = await eda.sch_PrimitiveComponent.createNetPort('BI','SPI1_SCLK',700,500);
const pico = await eda.sch_PrimitiveComponent.createNetPort('OUT','SPI1_PICO',700,490);
const poci = await eda.sch_PrimitiveComponent.createNetPort('IN','SPI1_POCI',700,480);
const cs0  = await eda.sch_PrimitiveComponent.createNetPort('OUT','SPI1_CS0',730,450);
const cs1  = await eda.sch_PrimitiveComponent.createNetPort('OUT','SPI1_CS1',730,440);
```

- [ ] **Step 2: Get current R48/R49 pin coordinates**

```javascript
const r48p = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<R48_ID>");
const r49p = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<R49_ID>");
return {
  r48_SCLK: r48p?.find(p=>p.name?.includes('SCLK')), r48_MOSI: r48p?.find(p=>p.name?.includes('MOSI')),
  r48_MISO: r48p?.find(p=>p.name?.includes('MISO')), r48_CS: r48p?.find(p=>p.name?.includes('CS')),
  r49_SCLK: r49p?.find(p=>p.name?.includes('SCLK')), r49_MOSI: r49p?.find(p=>p.name?.includes('MOSI')),
  r49_MISO: r49p?.find(p=>p.name?.includes('MISO')), r49_CS: r49p?.find(p=>p.name?.includes('CS'))
};
```

- [ ] **Step 3: Wire shared bus (SCLK/MOSI/MISO)**

For each of SCLK, MOSI, MISO: wire R48 pin → net port, AND R49 pin → same net/wire junction, all converging at the net port.

- [ ] **Step 4: Wire individual CS lines**

R48 CS → SPI1_CS0 net port. R49 CS → SPI1_CS1 net port. NO sharing.

- [ ] **Step 5: Mark unused pins No Connect**

```javascript
for (const id of ["<R48_ID>","<R49_ID>"]) {
  const pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId(id);
  for (const p of pins) {
    const nm = (p.name||'').toLowerCase();
    if (nm.includes('int')||nm.includes('fsync')||nm.includes('clkin')) {
      const ap = p.toAsync(); ap.setState_NoConnected(true); ap.done();
    }
  }
}
```

- [ ] **Step 6: Verify SPI wiring**

```javascript
const wires = await eda.sch_PrimitiveWire.getAll();
const spiNets = ['SPI1_SCLK','SPI1_PICO','SPI1_POCI','SPI1_CS0','SPI1_CS1'];
return spiNets.map(n=>({net:n, count:wires.filter(w=>w.net===n).length}));
```
Expected: >=1 wire per net.

- [ ] **Step 7: Commit**

```bash
git add . && git commit -m "task7: wire dual ICM42688 SPI1 with individual CS"
```

---

### Task 8: Place motor/encoder net labels

**Purpose:** Create all 21 net ports for motor control and encoder signals as intermediate wiring points between P1/P2 and TB6612 connectors.

- [ ] **Step 1: Place PWM net ports (x=400, y=600, spacing=20)**

```javascript
const pwms = ['M1_PWM','M2_PWM','M3_PWM','M4_PWM'];
for (let i=0; i<pwms.length; i++)
  await eda.sch_PrimitiveComponent.createNetPort('OUT', pwms[i], 400+i*20, 600);
```

- [ ] **Step 2: Place direction control net ports (x=400, y=620, spacing=20)**

```javascript
const dirs = ['M1_IN1','M1_IN2','M2_IN1','M2_IN2','M3_IN1','M3_IN2','M4_IN1','M4_IN2','TB6612_STBY'];
for (let i=0; i<dirs.length; i++)
  await eda.sch_PrimitiveComponent.createNetPort('OUT', dirs[i], 400+i*20, 620);
```

- [ ] **Step 3: Place encoder net ports (x=400, y=640, spacing=20)**

```javascript
const encs = ['ENC1_A','ENC1_B','ENC2_A','ENC2_B','ENC3_A','ENC3_B','ENC4_A','ENC4_B'];
for (let i=0; i<encs.length; i++)
  await eda.sch_PrimitiveComponent.createNetPort('IN', encs[i], 400+i*20, 640);
```

- [ ] **Step 4: Verify all created**

```javascript
const comps = await eda.sch_PrimitiveComponent.getAll();
const ports = comps.filter(c=>c.componentType==='NetPort');
return {count:ports.length, names:ports.map(p=>p.name).sort()};
```
Expected: 21 net ports.

- [ ] **Step 5: Commit**

```bash
git add . && git commit -m "task8: place motor/encoder net labels"
```

---

### Task 9: Wire M0 headers to TB6612 connectors

**Purpose:** Wire P1/P2 pins through net ports to J4/J6 connector pins.

**Depends on:** Task 1 (P1/P2 pins), Task 4 (J4/J6 pins), Task 8 (net ports)

- [ ] **Step 1: Wire P2 pins to motor net ports**

Using Task 1+8 data, wire from each P2 pin to its corresponding net port.
P2 signals to wire: H2-2(PA27/ENC3_A), H2-3(PA26/ENC2_B), H2-4(PA25/M4_PWM), H2-5(PA24/M3_PWM), H2-6(PA23/STBY), H2-7(PA22/M2_PWM), H2-8(PA21/M1_PWM), H2-9(PB9/M3_IN2), H2-10(PB8/M2_IN1), H2-14(PA15/M3_IN1), H2-15(PA14/ENC2_A), H2-16(PA13/ENC1_B), H2-17(PA12/ENC1_A).

- [ ] **Step 2: Wire P1 pins to motor net ports**

P1 signals: H1-3(PA28/ENC3_B), H1-4(PA31/ENC4_A), H1-7(PB24/M4_IN2), H1-9(PB19/M4_IN1), H1-10(PB18/ENC4_B), H1-11(PA7/M2_IN2), H1-16(PB6/M1_IN1), H1-17(PB7/M1_IN2).

- [ ] **Step 3: Wire motor net ports to J4 pins**

Wire to J4 pins per spec table: PWMA→J4-PWMA, PWMB→J4-PWMB, AIN1→J4-AIN1, AIN2→J4-AIN2, BIN1→J4-BIN1, BIN2→J4-BIN2, STBY→J4-STBY, E1A→J4-E1A, E1B→J4-E1B, E2A→J4-E2A, E2B→J4-E2B.

- [ ] **Step 4: Wire motor net ports to J6 pins**

Wire to J6: PWMC→J6-PWMC, PWMD→J6-PWMD, CIN1→J6-CIN1, CIN2→J6-CIN2, DIN1→J6-DIN1, DIN2→J6-DIN2, E3A→J6-E3A, E3B→J6-E3B, E4A→J6-E4A, E4B→J6-E4B, plus GND→J6-GND.

- [ ] **Step 5: Verify all motor nets wired**

```javascript
const wires = await eda.sch_PrimitiveWire.getAll();
const nets = ['M1_PWM','M2_PWM','M3_PWM','M4_PWM',
  'M1_IN1','M1_IN2','M2_IN1','M2_IN2','M3_IN1','M3_IN2','M4_IN1','M4_IN2',
  'TB6612_STBY','ENC1_A','ENC1_B','ENC2_A','ENC2_B','ENC3_A','ENC3_B','ENC4_A','ENC4_B'];
const missing = nets.filter(n=>!wires.some(w=>w.net===n));
return {totalNets:nets.length, missing};
```
Expected: 0 missing nets.

- [ ] **Step 6: Commit**

```bash
git add . && git commit -m "task9: wire M0 headers to TB6612 connectors"
```

---

### Task 10: Wire 3V3_IN from J4 to power circuit

**Purpose:** Connect J4-3V3 pin to +3V3_IN net port.

- [ ] **Step 1: Find J4 3V3 pin and wire to +3V3_IN**

```javascript
const j4p = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<J4_ID>");
const v3v3 = j4p?.find(p=>p.number==='1'||p.name?.toLowerCase().includes('3v3'));
// Wire v3v3 coordinates to +3V3_IN net port coordinates
```

```bash
git add . && git commit -m "task10: wire J4 3V3 to expansion board power"
```

---

### Task 11: Wire UART1 servo interface

**Purpose:** Connect ServoPWR 10V, UART1 TX/RX from P1, mark NC pins.

**Depends on:** Task 1 (Servo/ServoPWR/P1 pins), Task 3 (GND)

- [ ] **Step 1: Get Servo and ServoPWR pin details**

```javascript
const sp = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<Servo_ID>");
const spp = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<ServoPWR_ID>");
return {servo:sp?.map(p=>({n:p.number,x:p.x,y:p.y,name:p.name})), servoPWR:spp?.map(p=>({n:p.number,x:p.x,y:p.y,name:p.name}))};
```

- [ ] **Step 2: Place servo net labels**

```javascript
const rx = await eda.sch_PrimitiveComponent.createNetPort('OUT','SERVO_RX',400,280);
const tx = await eda.sch_PrimitiveComponent.createNetPort('IN','SERVO_TX',430,280);
const p10 = await eda.sch_PrimitiveComponent.createNetFlag('Power','+10V_SERVO',400,260);
```

- [ ] **Step 3: Wire ServoPWR 10V/GND to +10V_SERVO and GND**

- [ ] **Step 4: Wire Servo pins to +10V_SERVO (Pin1), GND (Pin2)**

- [ ] **Step 5: Wire P1 UART to servo data**

Wire PA8(H1-14) → SERVO_RX → Servo Pin5 (servoRx). Wire Servo Pin6 (servoTx) → SERVO_TX → PA9(H1-15).

- [ ] **Step 6: Mark servo pins 3/4 as No Connect**

```javascript
const sp = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId("<Servo_ID>");
for (const p of sp) { if (p.number==='3'||p.number==='4') { const a=p.toAsync(); a.setState_NoConnected(true); a.done(); } }
```

- [ ] **Step 7: Commit**

```bash
git add . && git commit -m "task11: wire UART1 servo with 10V power"
```

---

### Task 12: Wire I2C OLED and remaining SPI to P1/P2

**Purpose:** Complete I2C wiring with pull-ups, and connect SPI bus signals to P1/P2 headers.

- [ ] **Step 1: Place I2C net ports at (620,200) and (620,170)**

```javascript
await eda.sch_PrimitiveComponent.createNetPort('BI','I2C0_SDA',620,200);
await eda.sch_PrimitiveComponent.createNetPort('OUT','I2C0_SCL',620,170);
```

- [ ] **Step 2: Wire P1-H1-1 (PA0) to I2C0_SDA, P1-H1-2 (PA1) to I2C0_SCL**

- [ ] **Step 3: Wire J_I2C connector**

J_I2C Pin1(y-top)→GND, Pin2→+3V3_EXT, Pin3→I2C0_SDA, Pin4(y-bottom)→I2C0_SCL.

- [ ] **Step 4: Wire pull-up resistors: R_SDA(+3V3_EXT→SDA), R_SCL(+3V3_EXT→SCL)**

- [ ] **Step 5: Wire P2 SPI pins to SPI net ports**

P2-H2-12(PA17)→SPI1_SCLK, P2-H2-11(PA18)→SPI1_PICO, P2-H2-13(PA16)→SPI1_POCI.

- [ ] **Step 6: Wire P1 CS pins to SPI net ports**

P1-H1-8(PB20)→SPI1_CS0, P1-H1-6(PA2)→SPI1_CS1.

- [ ] **Step 7: Verify all I2C and SPI nets**

```javascript
const wires = await eda.sch_PrimitiveWire.getAll();
const allNets = ['I2C0_SDA','I2C0_SCL','SPI1_SCLK','SPI1_PICO','SPI1_POCI','SPI1_CS0','SPI1_CS1'];
const missing = allNets.filter(n=>!wires.some(w=>w.net===n));
return {missing};
```
Expected: 0 missing.

- [ ] **Step 8: Commit**

```bash
git add . && git commit -m "task12: wire I2C OLED and SPI to M0 headers"
```

---

### Task 13: Final audit, cleanup, and annotations

**Purpose:** Verify all connections, check decoupling caps, add PCB guidance text.

- [ ] **Step 1: Full connection completeness audit**

```javascript
const wires = await eda.sch_PrimitiveWire.getAll();
const comps = await eda.sch_PrimitiveComponent.getAll();
const allNets = [...new Set(wires.map(w=>w.net).filter(Boolean))];
const designators = comps.map(c=>c.designator).filter(Boolean);
return {components:designators.sort(), wireCount:wires.length, netCount:allNets.length, nets:allNets.sort()};
```

- [ ] **Step 2: Verify C18 placement near R48/R49**

C18 instances at (615,670) and (620,540) should be near ICM42688 (785-790, 560-690). If far, reposition:
```javascript
// Get C18 instances, move closer to ICMs using toAsync().setState_X/Y().done()
```

- [ ] **Step 3: Identify and handle unnamed components at (980,660)/(985,530)**

If these are ICM sub-symbols, verify connected. If not, note for manual check.

- [ ] **Step 4: Add PCB layout annotations**

```javascript
// Add text notes for PCB:
const t1 = await eda.sch_PrimitiveText.create("10V舵机电源 PCB走线>=1.5mm", 440, 240, "", 0, false);
const t2 = await eda.sch_PrimitiveText.create("自锁按键开关", 350, 170, "", 0, false);
```

- [ ] **Step 5: Commit final audit**

```bash
git add . && git commit -m "task13: final schematic audit and annotations"
```

---

## Completion Checklist

- [ ] All BOM components placed (16+ types)
- [ ] 3.3V power path complete: J4→SW1→+3V3_EXT→LED1+C_BULK+loads
- [ ] Dual ICM42688 share SPI1_SCLK/PICO/POCI, individual CS0/CS1
- [ ] 20 motor control/encoder nets wired between M0 headers and TB6612
- [ ] I2C OLED wired with pull-ups (GND,VCC,SDA,SCL)
- [ ] UART1 servo wired with 10V pass-through, NC pins marked
- [ ] All ICM42688 INT/FSYNC/CLKIN pins No Connect
- [ ] No net short circuits or floating signals
- [ ] 10V servo trace annotated for >=1.5mm PCB width
