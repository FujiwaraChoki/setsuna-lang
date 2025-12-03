import type { BaseLayoutProps } from 'fumadocs-ui/layouts/shared';

export function baseOptions(): BaseLayoutProps {
  return {
    nav: {
      title: 'Setsuna',
    },
    links: [
      {
        text: 'GitHub',
        url: 'https://github.com/FujiwaraChoki/setsuna-lang',
        active: 'nested-url',
      },
    ],
  };
}
