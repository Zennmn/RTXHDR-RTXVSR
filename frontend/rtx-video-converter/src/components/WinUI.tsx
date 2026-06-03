import React from 'react';

// Custom WinUI 3 inspired components
export function WinSlider({ 
  label, value, min, max, step, onChange, formatValue 
}: { 
  label: string, 
  value: number, 
  min: number, 
  max: number, 
  step: number, 
  onChange: (val: number) => void,
  formatValue?: (val: number) => string 
}) {
  return (
    <div>
      <div className="flex justify-between text-xs mb-2">
        <span className="text-[#1a1a1a]">{label}</span>
        <span className="font-mono text-[#1a1a1a]">{formatValue ? formatValue(value) : value}</span>
      </div>
      <input 
        type="range" min={min} max={max} step={step} value={value} 
        onChange={e => onChange(Number(e.target.value))}
        className="w-full accent-[#0067c0] cursor-pointer"
      />
    </div>
  )
}

export function WinSegmentedControl({ 
  options, value, onChange 
}: { 
  options: {label: string, value: string}[], 
  value: string, 
  onChange: (val: string) => void 
}) {
  return (
    <div className="flex bg-[#eeeeee] p-1 rounded-lg self-start w-full sm:w-auto">
      {options.map(opt => (
        <button
          key={opt.value}
          onClick={() => onChange(opt.value)}
          className={`flex-1 sm:flex-none px-8 py-1.5 text-xs rounded transition-all duration-200 font-medium ${
            value === opt.value 
              ? 'bg-white shadow-sm text-[#1a1a1a]' 
              : 'text-[#5a5a5a] hover:bg-black/5 hover:text-[#1a1a1a]'
          }`}
        >
          {opt.label}
        </button>
      ))}
    </div>
  )
}

export function WinSwitch({ 
  label, checked, onChange 
}: { 
  label: string, 
  checked: boolean, 
  onChange: (val: boolean) => void 
}) {
  return (
    <label className="flex items-center gap-1 cursor-pointer group w-max">
      <input 
        type="checkbox" 
        checked={checked} 
        onChange={e => onChange(e.target.checked)}
        className="w-3.5 h-3.5 accent-[#0067c0] cursor-pointer rounded-sm"
      />
      <span className="text-xs text-[#1a1a1a] group-hover:text-black transition-colors">{label}</span>
    </label>
  )
}
