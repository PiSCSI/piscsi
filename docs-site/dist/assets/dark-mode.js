(() => {
  const root = document.documentElement;
  const button = document.querySelector('#theme-toggle');
  button?.addEventListener('click', () => {
    const theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
    root.dataset.theme = theme;
    localStorage.setItem('piscsi-theme', theme);
  });
})();
