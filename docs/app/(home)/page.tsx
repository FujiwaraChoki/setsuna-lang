import Link from 'next/link';

export default function HomePage() {
  return (
    <div className="flex flex-col justify-center text-center flex-1 px-4">
      <h1 className="text-4xl font-bold mb-4">Setsuna</h1>
      <p className="text-xl mb-8">
        A modern functional programming language with clean syntax and powerful type inference
      </p>
      <div className="flex gap-4 justify-center">
        <Link
          href="/docs"
          className="px-6 py-3 bg-primary text-primary-foreground rounded-lg font-medium hover:opacity-90"
        >
          Get Started
        </Link>
        <Link
          href="https://github.com/FujiwaraChoki/setsuna-lang"
          className="px-6 py-3 border rounded-lg font-medium hover:bg-accent"
        >
          View on GitHub
        </Link>
      </div>
    </div>
  );
}
