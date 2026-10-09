// Lexiw: one behaviour for every table of the panel (base.html loads it).
//
//   - a click on a column's header sorts by it: descending, ascending, back
//     to the page's own order; numbers are read as numbers ("44 614 744",
//     "14,5%", "Lv 18", "13 h"), dates as dates,
//   - a table with a "Klasa" column gets the class buttons above it, unless
//     the page filters by class itself (data-server-class-filter),
//   - a table of 15 rows or more gets a box that hides the rows without
//     the text typed in it.
//
// Left alone: a table inside a form or with fields in its rows (sorting
// would move the fields), a table whose rows are not one cell per column
// (a row spanning columns, a detail row under its row), and anything under
// data-table-tools="off". Tables a page adds later are found too.
(function () {
  'use strict';

  const CLASSES = ['Wojownik', 'Ninja', 'Sura', 'Szaman'];
  const FILTER_FROM_ROWS = 15;
  const done = new WeakSet();

  const fold = (text) => text.toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/ł/g, 'l');
  const cellText = (cell) => (cell ? cell.textContent.replace(/\s+/g, ' ').trim() : '');

  // A cell's value for sorting: a number, a date, or the folded text.
  function readDate(text) {
    let m = text.match(/^(\d{1,2})\.(\d{1,2})\.(\d{4})(?:[ ,]+(\d{1,2}):(\d{2}))?/);
    if (m) return new Date(+m[3], m[2] - 1, +m[1], +(m[4] || 0), +(m[5] || 0)).getTime();
    m = text.match(/^(\d{4})-(\d{2})-(\d{2})(?:[ T](\d{2}):(\d{2}))?/);
    if (m) return new Date(+m[1], m[2] - 1, +m[3], +(m[4] || 0), +(m[5] || 0)).getTime();
    return null;
  }
  function readNumber(text) {
    // At most a short prefix before the number ("Lv", "#", "x", "+") and a
    // short unit after it ("%", "h", "yang", "szt.") - a name with digits
    // in it ("Wariat99") is text. Thousands are split by spaces in this
    // panel ("44 614 744"); a comma or a dot is the decimal point ("14,5%",
    // "91.3%").
    const m = text.replace(/ /g, ' ').match(/^([^\d+-]{0,3}?)\s*([+-]?\d{1,3}(?: \d{3})+|[+-]?\d+)(?:[.,](\d+))?\s*(.*)$/);
    if (!m || m[4].length > 8 || /\d/.test(m[4])) return null;
    const whole = m[2].replace(/ /g, '');
    return parseFloat(whole + (m[3] ? '.' + m[3] : ''));
  }

  function columnKind(rows, index) {
    let numbers = 0, dates = 0, filled = 0;
    rows.forEach((row) => {
      const text = cellText(row.cells[index]);
      if (!text || text === '—' || text === '-') return;
      filled += 1;
      if (readDate(text) !== null) dates += 1;
      else if (readNumber(text) !== null) numbers += 1;
    });
    if (!filled) return 'text';
    if (dates / filled >= 0.8) return 'date';
    if (numbers / filled >= 0.8) return 'number';
    return 'text';
  }

  function sortValue(cell, kind) {
    if (cell && cell.dataset.sort !== undefined) {
      const raw = cell.dataset.sort;
      return kind === 'text' ? fold(raw) : parseFloat(raw);
    }
    const text = cellText(cell);
    if (kind === 'date') return readDate(text);
    if (kind === 'number') return readNumber(text);
    return text === '—' ? null : fold(text);
  }

  function usable(table) {
    if (done.has(table) || table.closest('[data-table-tools="off"], form')) return null;
    const body = table.tBodies[0];
    if (!body || body.rows.length < 2) return null;
    if (body.querySelector('input, select, textarea, button')) return null;
    const head = table.tHead && table.tHead.rows.length === 1 ? table.tHead.rows[0] : null;
    if (!head || [...head.cells].some((th) => th.colSpan > 1)) return null;
    const width = head.cells.length;
    if (table.tBodies.length > 1 || [...body.rows].some((row) => row.cells.length !== width)) return null;
    return {body, head};
  }

  // The rows the class buttons and the text box leave visible, read live: a
  // page that refreshes its rows in place is filtered as it is now.
  function applyFilters(state) {
    const term = fold(state.term || '');
    const rows = [...state.body.rows];
    let shown = 0;
    rows.forEach((row) => {
      if (state.clsIndex >= 0 && row.dataset.ttClass === undefined) {
        const first = cellText(row.cells[state.clsIndex]).split(' ')[0];
        row.dataset.ttClass = CLASSES.includes(first) ? first : '';
      }
      const byClass = !state.cls || row.dataset.ttClass === state.cls;
      const byText = !term || fold(row.textContent).includes(term);
      const visible = byClass && byText;
      row.hidden = !visible;
      if (visible) shown += 1;
    });
    if (state.counter) {
      state.counter.textContent = shown === rows.length ? rows.length + ' wierszy' : shown + ' z ' + rows.length;
    }
  }

  function addSorting(table, parts, state) {
    const rows = [...parts.body.rows];
    rows.forEach((row, i) => { row.dataset.ttOrder = i; });
    [...parts.head.cells].forEach((th, index) => {
      if (!cellText(th) || th.dataset.sort === 'off') return;
      const kind = columnKind(rows, index);
      if (kind === 'number') {
        // Numbers line up on the right, the column's header with them.
        th.classList.add('tt-num');
        rows.forEach((row) => row.cells[index] && row.cells[index].classList.add('tt-num'));
      }
      th.classList.add('tt-sortable');
      th.tabIndex = 0;
      th.setAttribute('aria-sort', 'none');
      th.title = 'Kliknij, aby sortować';
      const sort = () => {
        const next = {none: 'descending', descending: 'ascending', ascending: 'none'}[th.getAttribute('aria-sort')];
        [...parts.head.cells].forEach((other) => other.setAttribute('aria-sort', 'none'));
        th.setAttribute('aria-sort', next);
        const ordered = [...parts.body.rows];
        if (next === 'none') {
          ordered.sort((a, b) => a.dataset.ttOrder - b.dataset.ttOrder);
        } else {
          const sign = next === 'ascending' ? 1 : -1;
          ordered.sort((a, b) => {
            const x = sortValue(a.cells[index], kind), y = sortValue(b.cells[index], kind);
            // Empty cells ("—") last, whichever way.
            if (x === null || Number.isNaN(x)) return (y === null || Number.isNaN(y)) ? a.dataset.ttOrder - b.dataset.ttOrder : 1;
            if (y === null || Number.isNaN(y)) return -1;
            if (x < y) return -sign;
            if (x > y) return sign;
            return a.dataset.ttOrder - b.dataset.ttOrder;
          });
        }
        const fragment = document.createDocumentFragment();
        ordered.forEach((row) => fragment.appendChild(row));
        parts.body.appendChild(fragment);
      };
      th.addEventListener('click', sort);
      th.addEventListener('keydown', (event) => {
        if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); sort(); }
      });
    });
    state.rows = rows;
  }

  function classColumn(head) {
    return [...head.cells].findIndex((th) => /^klasa$/i.test(cellText(th)));
  }

  function toolbar(table) {
    const bar = document.createElement('div');
    bar.className = 'tt-bar';
    table.parentNode.insertBefore(bar, table);
    return bar;
  }

  function addClassFilter(bar, state, index) {
    let found = 0;
    state.rows.forEach((row) => {
      const first = cellText(row.cells[index]).split(' ')[0];
      if (CLASSES.includes(first)) { row.dataset.ttClass = first; found += 1; }
    });
    if (found < 2) return;
    const group = document.createElement('div');
    group.className = 'tt-classes';
    group.setAttribute('role', 'group');
    group.setAttribute('aria-label', 'Filtr klas');
    ['', ...CLASSES].forEach((cls) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.textContent = cls || 'Wszystkie';
      button.setAttribute('aria-pressed', cls ? 'false' : 'true');
      button.addEventListener('click', () => {
        state.cls = cls;
        group.querySelectorAll('button').forEach((b) => b.setAttribute('aria-pressed', b === button ? 'true' : 'false'));
        applyFilters(state);
      });
      group.appendChild(button);
    });
    bar.appendChild(group);
  }

  function addTextFilter(bar, state) {
    const label = document.createElement('label');
    label.className = 'tt-search';
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder = 'Filtruj tabelę…';
    input.setAttribute('aria-label', 'Filtruj wiersze tabeli');
    input.addEventListener('input', () => { state.term = input.value; applyFilters(state); });
    const counter = document.createElement('small');
    label.append(input, counter);
    bar.appendChild(label);
    state.counter = counter;
  }

  function enhance(table) {
    const parts = usable(table);
    if (!parts) return;
    done.add(table);
    table.classList.add('tt-table');
    const state = {body: parts.body, rows: [], cls: '', term: '', clsIndex: -1};
    addSorting(table, parts, state);
    const serverClasses = document.querySelector('[data-server-class-filter]');
    const clsIndex = serverClasses ? -1 : classColumn(parts.head);
    const wantsText = state.rows.length >= FILTER_FROM_ROWS;
    if (clsIndex < 0 && !wantsText) return;
    const bar = toolbar(table);
    if (clsIndex >= 0) { state.clsIndex = clsIndex; addClassFilter(bar, state, clsIndex); }
    if (wantsText) addTextFilter(bar, state);
    if (!bar.childElementCount) bar.remove();
    else applyFilters(state);
  }

  function scan(root) {
    (root.querySelectorAll ? root.querySelectorAll('table') : []).forEach(enhance);
    if (root.tagName === 'TABLE') enhance(root);
  }

  function start() {
    scan(document);
    let pending = null;
    new MutationObserver(() => {
      if (pending) return;
      pending = setTimeout(() => {
        pending = null;
        scan(document);
      }, 150);
    }).observe(document.body, {childList: true, subtree: true});
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
  else start();
})();
