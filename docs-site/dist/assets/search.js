(() => {
  const input = document.querySelector('#search-input');
  const results = document.querySelector('#search-results');
  if (!input || !results) return;

  fetch('search-index.json').then((response) => response.json()).then((pages) => {
    input.addEventListener('input', () => {
      const query = input.value.trim().toLowerCase();
      if (!query) { results.hidden = true; results.innerHTML = ''; return; }
      const matches = pages.filter((page) => `${page.title} ${page.description} ${page.text}`.toLowerCase().includes(query)).slice(0, 8);
      results.innerHTML = matches.length ? matches.map((page) => `<a href="${page.url}"><strong>${page.title}</strong><small>${page.description}</small></a>`).join('') : '<span class="no-results">No matching pages</span>';
      results.hidden = false;
    });
  }).catch(() => { input.disabled = true; });
})();
