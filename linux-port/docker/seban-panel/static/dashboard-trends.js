// Lexiw: the dashboard's row of 24-hour mini charts (/api/dashboard-trends).
// Drawn after the page is shown, as the other dashboard widgets are; a chart
// with no data says so instead of drawing an empty line.
(function () {
  'use strict';
  const root = document.getElementById('dashboardTrends');
  if (!root || typeof Chart === 'undefined') return;

  const number = (value) => Math.round(value).toLocaleString('pl-PL');
  const compact = (value) => {
    if (Math.abs(value) >= 1e6) return (value / 1e6).toLocaleString('pl-PL', {maximumFractionDigits: 1}) + ' mln';
    if (Math.abs(value) >= 1e4) return (value / 1e3).toLocaleString('pl-PL', {maximumFractionDigits: 1}) + ' tys.';
    return number(value);
  };
  const sum = (values) => values.reduce((a, b) => a + b, 0);
  const colour = (name, fallback) =>
    (getComputedStyle(document.body).getPropertyValue(name) || '').trim() || fallback;

  function card(key) { return root.querySelector('[data-trend="' + key + '"]'); }

  function fill(key, value, detail) {
    const el = card(key);
    if (!el) return;
    el.querySelector('strong').textContent = value;
    el.querySelector('small').textContent = detail;
    el.classList.remove('is-loading');
  }

  function draw(key, series, type, tooltip) {
    const el = card(key);
    const canvas = el && el.querySelector('canvas');
    if (!canvas) return;
    if (!series || !series.values.length) {
      el.classList.add('is-empty');
      return;
    }
    const line = colour('--gold', '#e8b93f');
    new Chart(canvas, {
      type: type,
      data: {
        labels: series.labels,
        datasets: [{
          data: series.values,
          borderColor: line,
          backgroundColor: type === 'bar' ? line + '99' : line + '22',
          fill: type !== 'bar',
          tension: 0.35,
          pointRadius: 0,
          borderWidth: type === 'bar' ? 0 : 2,
          borderRadius: 2,
        }],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
          legend: {display: false},
          tooltip: {
            displayColors: false,
            callbacks: {label: (ctx) => tooltip(ctx.parsed.y, ctx.dataIndex)},
          },
        },
        scales: {x: {display: false}, y: {display: false, beginAtZero: type === 'bar'}},
      },
    });
  }

  fetch((window.SEBAN_ROOT || '') + '/api/dashboard-trends', {cache: 'no-store'})
    .then((response) => response.json())
    .then((data) => {
      if (!data.ok) throw new Error('no data');

      const bots = data.bots.values;
      if (bots.length) {
        fill('bots', number(bots[bots.length - 1]), 'najwięcej w 24 h: ' + number(Math.max(...bots)));
      } else {
        fill('bots', '—', 'kolektor jeszcze nie zebrał danych');
      }
      draw('bots', data.bots, 'line', (v) => number(v) + ' botów');

      const yang = data.yang.values;
      if (yang.length) {
        const first = yang[0], last = yang[yang.length - 1];
        const change = first ? ((last - first) / first) * 100 : 0;
        const sign = change > 0 ? '+' : '';
        fill('yang', compact(last), sign + change.toLocaleString('pl-PL', {maximumFractionDigits: 1}) + '% w 24 h');
      } else {
        fill('yang', '—', 'kolektor jeszcze nie zebrał danych');
      }
      draw('yang', data.yang, 'line', (v) => number(v) + ' yang');

      const refines = sum(data.refines.values);
      const ok = sum(data.refines.ok);
      fill('refines', compact(refines),
        refines ? 'udanych ' + Math.round((ok / refines) * 100) + '% · na godzinę' : 'brak ulepszeń w 24 h');
      draw('refines', data.refines, 'bar', (v, i) =>
        number(v) + ' ulepszeń, udanych ' + number(data.refines.ok[i]));

      const deaths = sum(data.deaths.values);
      fill('deaths', compact(deaths), 'w tym PvP: ' + number(sum(data.deaths.pvp)) + ' · na godzinę');
      draw('deaths', data.deaths, 'bar', (v, i) =>
        number(v) + ' zgonów' + (data.deaths.pvp[i] ? ', PvP: ' + number(data.deaths.pvp[i]) : ''));
    })
    .catch(() => {
      root.querySelectorAll('[data-trend]').forEach((el) => {
        el.classList.remove('is-loading');
        el.classList.add('is-empty');
        el.querySelector('small').textContent = 'nie udało się wczytać';
      });
    });
})();
